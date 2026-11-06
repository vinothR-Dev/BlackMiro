#pragma once

// Wire frame (binary WebSocket message): 1 type byte + payload.
// The payload is end-to-end encrypted by clients, so the relay never sees board content.
namespace RelayFrame {
enum Type : char {
    Ops = 1,           // logged by the relay and forwarded
    Snapshot = 2,      // replaces the room log (full state); not forwarded
    Presence = 3,      // forwarded only (cursors, names, selections)
    WantSnapshot = 9,  // relay -> client: please send a snapshot (log is getting long)
};
}
