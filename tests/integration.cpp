#include "wire.h"
#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
std::string fetch(unsigned short port, const std::string& path) {
    int fd = socket(AF_INET, SOCK_STREAM, 0); assert(fd >= 0);
    timeval timeout{1, 0}; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(port); address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof address) < 0) { close(fd); return {}; }
    const std::string request = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    assert(send(fd, request.data(), request.size(), 0) == static_cast<ssize_t>(request.size()));
    std::string result; char bytes[2048]; ssize_t count;
    while ((count = recv(fd, bytes, sizeof bytes, 0)) > 0) result.append(bytes, static_cast<std::size_t>(count));
    close(fd); return result;
}
int main() {
    int master = posix_openpt(O_RDWR | O_NOCTTY); assert(master >= 0);
    assert(grantpt(master) == 0 && unlockpt(master) == 0);
    std::string device = ptsname(master);
    int input[2]; assert(pipe(input) == 0);
    const auto port = static_cast<unsigned short>(20000 + getpid() % 30000);
    const auto port_text = std::to_string(port);
    pid_t pid = fork(); assert(pid >= 0);
    if (pid == 0) {
        close(master); close(input[1]); dup2(input[0], STDIN_FILENO); close(input[0]);
        int null = open("/dev/null", O_WRONLY); dup2(null, STDOUT_FILENO); close(null);
        execl("./build/bridge", "bridge", device.c_str(), port_text.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    close(input[0]);
    bool ready = false;
    for (unsigned i = 0; i < 100; ++i) {
        if (fetch(port, "/").find("Hardware Inventory Bridge") != std::string::npos) { ready = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!ready) { kill(pid, SIGTERM); waitpid(pid, nullptr, 0); }
    assert(ready);
    const wire_sample sample{0, 2000, 2534, 6500, 1, 1};
    uint8_t frame[WIRE_FRAME_SIZE]; wire_encode(&sample, frame);
    assert(write(master, frame, sizeof frame) == sizeof frame);
    const char commands[] = "move cable 12 delivery-1\nmove cable 12 delivery-1\nmove cable -3 withdrawal-1\n";
    assert(write(input[1], commands, std::strlen(commands)) == static_cast<ssize_t>(std::strlen(commands)));
    bool received = false;
    for (unsigned i = 0; i < 100; ++i) {
        const auto result = fetch(port, "/api/status");
        if (result.find("\"connected\":true") != std::string::npos && result.find("\"quantity\":9") != std::string::npos && result.find("\"temperature\":25.34") != std::string::npos) { received = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    const bool missing = fetch(port, "/missing").starts_with("HTTP/1.1 404");
    kill(pid, SIGINT); int status = 0; assert(waitpid(pid, &status, 0) == pid);
    close(input[1]); close(master);
    assert(received && missing && WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
