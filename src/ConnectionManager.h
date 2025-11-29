#pragma once

#include <string>
#include <cstdint>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

// Forward declarations for ENet
struct _ENetHost;
struct _ENetPeer;
struct _ENetEvent;
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;
typedef struct _ENetEvent ENetEvent;

enum class ConnectionType {
    NONE,
    DIRECT,              // Direct peer-to-peer connection
    NAT_PUNCHTHROUGH,    // Direct connection via NAT punchthrough
    RELAY                // Connection via ServerManager relay
};

// Connection info for a peer
struct PeerConnection {
    ENetPeer* peer;
    std::string identifier;  // "IP:PORT" or "RELAY:ROOMCODE"
    ConnectionType type;
    std::chrono::steady_clock::time_point lastHeartbeat;
    bool connected;
};

class ConnectionManager {
public:
    ConnectionManager();
    ~ConnectionManager();

    // Initialize networking (call once at startup)
    bool Initialize();

    // Cleanup networking (call once at shutdown)
    void Cleanup();

    // Start hosting (creates server)
    // Returns true on success
    bool StartHost(uint16_t port, uint32_t maxClients = 32);

    // Stop hosting
    void StopHost();

    // Connect to a remote host
    // Tries: Direct → NAT Punchthrough → Relay (automatic fallback)
    // Returns true on success
    bool ConnectToHost(const std::string& roomCode,
                      const std::string& serverManagerIP, uint16_t serverManagerPort,
                      const std::string& hostPublicIP, uint16_t hostPublicPort,
                      const std::string& hostLocalIP, uint16_t hostLocalPort);

    // Disconnect from host
    void DisconnectFromHost();

    // Send data to a specific peer (by identifier)
    // identifier format: "IP:PORT" for direct connections, "RELAY:ROOMCODE" for relay
    bool SendToPeer(const std::string& peerIdentifier, const void* data, size_t length, bool reliable = true);

    // Send data to the first connected peer (useful for client connections with only one peer)
    bool SendToFirstPeer(const void* data, size_t length, bool reliable = true);

    // Broadcast to all connected peers
    bool BroadcastToAllPeers(const void* data, size_t length, bool reliable = true);

    // Receive data
    // Returns true if data was received, false if no data available
    // Also handles connection/disconnection events internally
    bool Receive(void* buffer, size_t bufferSize, size_t& received, 
                std::string& fromPeerIdentifier);

    // Check if a new peer connected (call after Receive if needed)
    // Returns peer identifier if new connection, empty string otherwise
    std::string GetNewPeerConnection();

    // Check if a peer disconnected (call after Receive if needed)
    // Returns peer identifier if disconnected, empty string otherwise
    std::string GetDisconnectedPeer();

    // Update connection state (call every frame)
    void Update(float deltaTime);
    
    // Flush pending packets (ensures reliable messages are sent)
    void Flush();

    // Check connection status
    bool IsHosting() const { return isHosting; }
    bool IsConnected() const { return !connectedPeers.empty(); }
    ConnectionType GetConnectionType(const std::string& peerIdentifier) const;

    // Get number of connected peers
    size_t GetConnectedPeerCount() const;

    // Get the first connected peer identifier (for client connections)
    // Returns empty string if no peers connected
    std::string GetFirstConnectedPeerIdentifier() const;

    // Enable/disable relay mode
    void SetUseRelay(bool useRelay);
    bool IsUsingRelay() const { return useRelay; }

    // Get local IP address (if hosting)
    std::string GetLocalIP() const;

    // Get public IP address (query ServerManager or use detection)
    static std::string GetPublicIP();

private:
    // Connection strategy methods
    bool TryDirectConnection(const std::string& hostIP, uint16_t hostPort);
    bool TryNATPunchthrough(const std::string& roomCode,
                           const std::string& hostPublicIP, uint16_t hostPublicPort,
                           const std::string& hostLocalIP, uint16_t hostLocalPort);
    bool TryRelayConnection(const std::string& roomCode);

    // Helper methods
    ENetPeer* FindOrCreatePeer(const std::string& identifier);
    void RemovePeer(const std::string& identifier);
    void ProcessENetEvents();
    void CleanupDisconnectedPeers();

    // ServerManager communication (for relay/punchthrough)
    bool SendToServerManager(const void* data, size_t length);
    bool ReceiveFromServerManager(void* buffer, size_t bufferSize, size_t& received);
    void HandleServerManagerMessage(const void* data, size_t length);

    ENetHost* enetHost;
    // Note: ServerManager uses raw UDP (NetworkUtils), not ENet

    bool initialized;
    bool isHosting;
    bool useRelay;

    // Connection state
    std::unordered_map<std::string, PeerConnection> connectedPeers;
    mutable std::mutex peersMutex;

    // ServerManager info (for relay/punchthrough coordination)
    std::string serverManagerIP;
    uint16_t serverManagerPort;
    std::string currentRoomCode;

    // Connection timing
    std::chrono::steady_clock::time_point connectionStartTime;
    static constexpr auto DIRECT_TIMEOUT = std::chrono::seconds(2);
    static constexpr auto PUNCHTHROUGH_TIMEOUT = std::chrono::seconds(3);

    // Bandwidth tracking
    std::atomic<uint64_t> bytesSent;
    std::atomic<uint64_t> bytesReceived;
};

