# Future persistence structure

`inventory::Repository::commit(Movement)` is the inventory persistence boundary. `Ledger` prepares the balance and idempotency maps before calling the adapter; it publishes those maps after commit succeeds. The default adapter stores events in memory.

A future database requires inventory balances, movements and unique request keys. Atomically update the balance and insert the movement. Reject a request key reused with a different SKU or quantity. Multi-process services need database locks or optimistic versioning in addition to the process mutex. Add snapshot loading and restore sequence and request keys before accepting movements after restart.

Telemetry uses device session, sequence, uptime and received-at time. The current gateway retains only the latest frame and the browser retains up to 120 chart points. Add a bounded telemetry writer at the accepted-frame branch when persistent samples are required. Do not derive product identity or withdrawal quantity from PIR presence.

No database implementation, cloud account or connection string is provided.
