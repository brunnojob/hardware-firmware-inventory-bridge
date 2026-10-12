#include "inventory.hpp"
#include "wire.h"
#include "dashboard.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <chrono>
#include <cerrno>
#include <charconv>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>
using Clock = std::chrono::steady_clock;
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }
struct Client {
    int fd = -1;
    std::string input;
    std::string output;
    std::size_t written = 0;
    Clock::time_point deadline;
};
int serial_open(const char* path, termios& saved) {
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_NOCTTY);
    if (fd < 0) throw std::runtime_error(std::strerror(errno));
    termios config{};
    if (tcgetattr(fd, &saved)) { close(fd); throw std::runtime_error("cannot read serial configuration"); }
    config.c_cflag = CS8 | CREAD | CLOCAL;
    cfsetispeed(&config, B115200); cfsetospeed(&config, B115200);
    if (tcsetattr(fd, TCSANOW, &config)) { close(fd); throw std::runtime_error("cannot configure serial"); }
    return fd;
}
int listen_local(std::uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0) throw std::runtime_error("socket failed");
    int value = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof value);
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(port); address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof address) || listen(fd, 16)) { close(fd); throw std::runtime_error("listen failed"); }
    return fd;
}
std::string status_json(const inventory::Ledger& ledger, const wire_sample& sample, bool connected, std::uint64_t accepted, std::uint64_t rejected) {
    std::ostringstream out;
    out << "{\"connected\":" << (connected ? "true" : "false") << ",\"valid\":" << (sample.valid ? "true" : "false") << ",\"temperature\":" << sample.temperature / 100.0 << ",\"humidity\":" << sample.humidity / 100.0 << ",\"presence\":" << (sample.presence ? "true" : "false") << ",\"sequence\":" << sample.sequence << ",\"accepted\":" << accepted << ",\"rejected\":" << rejected << ",\"inventory\":[";
    bool first = true;
    for (const auto& [sku, quantity] : ledger.snapshot()) {
        if (!first) out << ',';
        first = false; out << "{\"sku\":\"" << sku << "\",\"quantity\":" << quantity << '}';
    }
    return out.str() + "]}";
}
std::string response(int code, const std::string& type, const std::string& body) {
    return "HTTP/1.1 " + std::to_string(code) + (code == 200 ? " OK\r\n" : " Error\r\n") + "Content-Type: " + type + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'\r\n\r\n" + body;
}
int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) { std::cerr << "usage: bridge SERIAL_DEVICE [PORT]\n"; return 2; }
    unsigned port = 8080;
    if (argc == 3) {
        const auto end = argv[2] + std::strlen(argv[2]);
        const auto result = std::from_chars(argv[2], end, port);
        if (result.ec != std::errc{} || result.ptr != end || port < 1024 || port > 65535) { std::cerr << "invalid port\n"; return 2; }
    }
    int serial = -1, server = -1; termios saved{};
    int input_flags = -1;
    std::array<Client, 16> clients;
    int exit_code = 0;
    try {
        serial = serial_open(argv[1], saved); server = listen_local(static_cast<std::uint16_t>(port));
        std::signal(SIGINT, stop); std::signal(SIGTERM, stop); std::signal(SIGPIPE, SIG_IGN);
        inventory::MemoryRepository repository; inventory::Ledger ledger(repository);
        wire_parser parser{}; wire_sample latest{};
        Clock::time_point last{}; bool have_sample = false;
        std::uint64_t accepted = 0, rejected = 0;
        bool input_open = true;
        std::string command_buffer;
        input_flags = fcntl(STDIN_FILENO, F_GETFL);
        if (input_flags < 0 || fcntl(STDIN_FILENO, F_SETFL, input_flags | O_NONBLOCK) < 0) throw std::runtime_error("cannot configure stdin");
        std::cout << "http://127.0.0.1:" << port << "\nmove SKU SIGNED_QUANTITY REQUEST_KEY\n" << std::flush;
        while (running) {
            std::array<pollfd, 19> descriptors{};
            descriptors[0] = {serial, POLLIN, 0}; descriptors[1] = {server, POLLIN, 0};
            descriptors[2] = {input_open ? STDIN_FILENO : -1, POLLIN, 0};
            for (std::size_t i = 0; i < clients.size(); ++i) descriptors[i + 3] = {clients[i].fd, static_cast<short>(clients[i].output.empty() ? POLLIN : POLLOUT), 0};
            int ready = poll(descriptors.data(), descriptors.size(), 100);
            if (ready < 0) { if (errno == EINTR) continue; throw std::runtime_error("poll failed"); }
            if (descriptors[0].revents & (POLLHUP | POLLERR | POLLNVAL)) { close(serial); serial = -1; have_sample = false; }
            if (descriptors[0].revents & POLLIN && serial >= 0) {
                std::uint8_t bytes[512]; const auto count = read(serial, bytes, sizeof bytes);
                for (ssize_t i = 0; i < count; ++i) {
                    wire_sample sample;
                    if (!wire_feed(&parser, bytes[i], &sample)) continue;
                    const std::uint32_t delta = sample.sequence - latest.sequence;
                    const std::uint32_t elapsed = sample.uptime - latest.uptime;
                    const bool forward = delta > 0 && delta < 0x80000000u && elapsed < 0x80000000u;
                    const bool restart = sample.sequence <= 2 && sample.uptime <= 10000 && elapsed >= 0x80000000u;
                    if (have_sample && !forward && !restart) { ++rejected; continue; }
                    latest = sample; last = Clock::now(); have_sample = true; ++accepted;
                }
            }
            if (descriptors[2].revents & (POLLIN | POLLHUP)) {
                char buffer[512]; const auto count = read(STDIN_FILENO, buffer, sizeof buffer);
                if (count == 0) input_open = false;
                if (count > 0) command_buffer.append(buffer, static_cast<std::size_t>(count));
                std::size_t end;
                while ((end = command_buffer.find('\n')) != std::string::npos) {
                    std::istringstream input(command_buffer.substr(0, end)); command_buffer.erase(0, end + 1);
                    std::string command, sku, key, extra; std::int32_t delta;
                    try {
                        if (!(input >> command >> sku >> delta >> key) || command != "move" || (input >> extra)) throw std::invalid_argument("move SKU SIGNED_QUANTITY REQUEST_KEY");
                        const auto movement = ledger.move(sku, delta, key);
                        std::cout << movement.sequence << ' ' << movement.sku << ' ' << movement.balance << '\n' << std::flush;
                    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
                }
                if (command_buffer.size() > 4096) { command_buffer.clear(); std::cerr << "command too long\n"; }
            }
            if (descriptors[1].revents & POLLIN) {
                const int fd = accept(server, nullptr, nullptr);
                if (fd >= 0) {
                    auto available = std::find_if(clients.begin(), clients.end(), [](const auto& c) { return c.fd < 0; });
                    if (available == clients.end() || fcntl(fd, F_SETFL, O_NONBLOCK) < 0) close(fd);
                    else { *available = Client{}; available->fd = fd; available->deadline = Clock::now() + std::chrono::seconds(2); }
                }
            }
            for (std::size_t i = 0; i < clients.size(); ++i) {
                auto& client = clients[i]; if (client.fd < 0) continue;
                const auto events = descriptors[i + 3].revents;
                bool close_client = Clock::now() > client.deadline || (events & (POLLHUP | POLLERR | POLLNVAL));
                if (events & POLLIN && !close_client) {
                    char buffer[1024]; const auto count = recv(client.fd, buffer, sizeof buffer, 0);
                    if (count <= 0) { if (count == 0 || (errno != EAGAIN && errno != EINTR)) close_client = true; }
                    else client.input.append(buffer, static_cast<std::size_t>(count));
                    if (client.input.size() > 4096) close_client = true;
                    if (client.input.find("\r\n\r\n") != std::string::npos) {
                        std::istringstream request(client.input); std::string method, path, version;
                        request >> method >> path >> version;
                        if (method != "GET" || (version != "HTTP/1.1" && version != "HTTP/1.0")) client.output = response(400, "text/plain", "invalid request");
                        else if (path == "/") client.output = response(200, "text/html; charset=utf-8", dashboard_html);
                        else if (path == "/api/status") client.output = response(200, "application/json", status_json(ledger, latest, have_sample && Clock::now() - last < std::chrono::seconds(6), accepted, rejected));
                        else client.output = response(404, "text/plain", "not found");
                    }
                }
                if (events & POLLOUT && !close_client) {
                    const auto count = send(client.fd, client.output.data() + client.written, client.output.size() - client.written, 0);
                    if (count > 0) client.written += static_cast<std::size_t>(count);
                    else if (count < 0 && errno != EAGAIN && errno != EINTR) close_client = true;
                    if (client.written == client.output.size()) close_client = true;
                }
                if (close_client) { close(client.fd); client = Client{}; }
            }
        }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; exit_code = 1; }
    if (input_flags >= 0) fcntl(STDIN_FILENO, F_SETFL, input_flags);
    for (auto& client : clients) if (client.fd >= 0) close(client.fd);
    if (server >= 0) close(server);
    if (serial >= 0) { tcsetattr(serial, TCSANOW, &saved); close(serial); }
    return exit_code;
}
