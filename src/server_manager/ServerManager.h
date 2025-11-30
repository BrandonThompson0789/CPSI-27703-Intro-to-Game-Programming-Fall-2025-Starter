#pragma once

#include "NetworkUtils.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>
#include <random>

struct RoomInfo {
    std::string roomCode;
    std::string hostIP;           // Local IP (for reference)
    uint16_t hostPort;            // Local port (from message)
    std::string hostPublicIP;     // Public IP (from NAT, detected by ServerManager)
    uint16_t hostPublicPort;      // Public port (from NAT, detected by ServerManager)
    int connectedPlayers;
    std::vector<std::string> playerIPs;
    std::chrono::steady_clock::time_point lastPing;
    std::chrono::steady_clock::time_point createdAt;
};

enum class MessageType : uint8_t {
    HOST_REGISTER = 1,
    HOST_HEARTBEAT = 2,
    HOST_UPDATE = 3,
    CLIENT_LOOKUP = 4,
    RESPONSE_REGISTER = 5,
    RESPONSE_ROOM_INFO = 6,
    RESPONSE_ERROR = 7,
    // NAT punchthrough coordination
    NAT_PUNCHTHROUGH_REQUEST = 8,
    NAT_PUNCHTHROUGH_RESPONSE = 9,
    // Relay coordination
    RELAY_REQUEST = 10,
    RELAY_RESPONSE = 11,
    RELAY_DECLINE = 12,
    RELAY_DATA = 13,
    // Path efficiency measurement
    PATH_TEST_REQUEST = 14,
    PATH_TEST_RESPONSE = 15
};

struct MessageHeader {
    MessageType type;
    uint8_t reserved[3];
};

struct HostRegisterMessage {
    MessageHeader header;
    uint16_t hostPort;
    char reserved[1];
    char hostIP[16]; // IPv4 max length (including null terminator)
};

struct HostHeartbeatMessage {
    MessageHeader header;
};

struct HostUpdateMessage {
    MessageHeader header;
    uint16_t playerCount;
    uint16_t reserved;
    // Followed by playerCount IP addresses as null-terminated strings
};

struct ClientLookupMessage {
    MessageHeader header;
    char roomCode[7]; // 6 chars + null terminator
};

struct RegisterResponse {
    MessageHeader header;
    char roomCode[7]; // 6 chars + null terminator
    uint16_t hostPort;
    char reserved[1];
};

struct RoomInfoResponse {
    MessageHeader header;
    uint16_t hostPort;        // Local port (for backward compatibility, may be same as public port)
    uint16_t hostPublicPort;  // Public port (from NAT)
    uint16_t playerCount;
    uint8_t forcedConnectionType;  // 0 = NONE, 1 = DIRECT_ONLY, 2 = NAT_ONLY, 3 = RELAY_ONLY
    uint8_t relayEnabled;     // 1 if relay enabled, 0 if disabled
    char reserved[2];
    char hostIP[16];          // Local IP (for reference)
    char hostPublicIP[16];    // Public IP (from NAT) - this is what clients should use
    // Followed by playerCount IP addresses as null-terminated strings
};

struct ErrorResponse {
    MessageHeader header;
    char errorMessage[64];
};

// NAT punchthrough request (from client to server manager)
struct NATPunchthroughRequest {
    MessageHeader header;
    char roomCode[7];  // 6 chars + null terminator
    char clientPublicIP[16];  // Client's public IP (detected by server manager)
    uint16_t clientPublicPort;  // Client's public port
    char clientLocalIP[16];  // Client's local IP
    uint16_t clientLocalPort;  // Client's local port
};

// NAT punchthrough response (from server manager to host and client)
struct NATPunchthroughResponse {
    MessageHeader header;
    char roomCode[7];
    // Host info
    char hostPublicIP[16];
    uint16_t hostPublicPort;
    char hostLocalIP[16];
    uint16_t hostLocalPort;
    // Client info
    char clientPublicIP[16];
    uint16_t clientPublicPort;
    char clientLocalIP[16];
    uint16_t clientLocalPort;
};

// Relay request (from client to server manager)
struct RelayRequest {
    MessageHeader header;
    char roomCode[7];
    char clientIP[16];
    uint16_t clientPort;
};

// Relay response (from server manager to client)
struct RelayResponse {
    MessageHeader header;
    char roomCode[7];
    uint8_t accepted;  // 1 if accepted, 0 if declined
    char reserved[2];
    uint16_t relayPort;  // Port to use for relay (if accepted)
};

// Relay decline (from server manager to host)
struct RelayDecline {
    MessageHeader header;
    char roomCode[7];
    char clientIP[16];
    uint16_t clientPort;
    char reason[64];
};

// Relay data (forwarded by server manager)
struct RelayDataHeader {
    MessageHeader header;
    char roomCode[7];
    char fromIP[16];
    uint16_t fromPort;
    char toIP[16];
    uint16_t toPort;
    uint32_t dataLength;
    // Followed by actual data
};

// Path test request (for measuring connection efficiency)
struct PathTestRequest {
    MessageHeader header;
    char roomCode[7];
    char testIP[16];
    uint16_t testPort;
    uint64_t timestamp;  // For latency measurement
};

// Path test response
struct PathTestResponse {
    MessageHeader header;
    char roomCode[7];
    uint64_t timestamp;  // Echo back timestamp
    uint32_t latencyMs;  // Measured latency
};

// Forced connection type (for testing)
enum class ForcedConnectionType {
    NONE,           // No forcing - use normal fallback
    DIRECT_ONLY,    // Only allow direct connections
    NAT_ONLY,       // Only allow NAT punchthrough
    RELAY_ONLY      // Only allow relay connections
};

class ServerManager {
public:
    ServerManager(uint16_t port = 8888);
    ~ServerManager();

    bool Initialize();
    void Run();
    void Shutdown();
    
    // Relay management
    bool IsRelayEnabled() const { return relayEnabled; }
    void SetRelayEnabled(bool enabled) { relayEnabled = enabled; }
    
    // Forced connection type (for testing)
    void SetForcedConnectionType(ForcedConnectionType type) { forcedConnectionType = type; }
    ForcedConnectionType GetForcedConnectionType() const { return forcedConnectionType; }

private:
    void ProcessMessage(const std::string& fromIP, uint16_t fromPort, 
                       const void* data, size_t length);
    
    void HandleHostRegister(const std::string& fromIP, uint16_t fromPort,
                           const HostRegisterMessage& msg);
    void HandleHostHeartbeat(const std::string& fromIP, uint16_t fromPort);
    void HandleHostUpdate(const std::string& fromIP, uint16_t fromPort,
                         const void* data, size_t length);
    void HandleClientLookup(const std::string& fromIP, uint16_t fromPort,
                           const ClientLookupMessage& msg);
    void HandleNATPunchthroughRequest(const std::string& fromIP, uint16_t fromPort,
                                     const NATPunchthroughRequest& msg);
    void HandleRelayRequest(const std::string& fromIP, uint16_t fromPort,
                           const RelayRequest& msg);
    void HandleRelayData(const std::string& fromIP, uint16_t fromPort,
                        const void* data, size_t length);
    void HandlePathTestRequest(const std::string& fromIP, uint16_t fromPort,
                              const PathTestRequest& msg);
    
    std::string GenerateRoomCode();
    void CleanupStaleRooms();
    void SendResponse(const std::string& toIP, uint16_t toPort,
                     const void* data, size_t length);
    
    // Active relay connections: roomCode -> (clientIP:port -> hostIP:port)
    struct RelayConnection {
        std::string clientIP;
        uint16_t clientPort;
        std::string hostIP;
        uint16_t hostPort;
        std::chrono::steady_clock::time_point lastActivity;
    };
    std::unordered_map<std::string, std::vector<RelayConnection>> activeRelays;  // Key: roomCode
    std::mutex relaysMutex;
    bool relayEnabled;
    ForcedConnectionType forcedConnectionType;

    SocketHandle m_socket;
    uint16_t m_port;
    bool m_running;
    
    std::unordered_map<std::string, RoomInfo> m_rooms; // Key: roomCode
    std::unordered_map<std::string, std::string> m_hostToRoom; // Key: hostIP:port, Value: roomCode
    std::mutex m_roomsMutex;
    
    std::random_device m_randomDevice;
    std::mt19937 m_randomGenerator;
    std::uniform_int_distribution<int> m_codeDistribution;
    
    static constexpr int ROOM_CODE_LENGTH = 6;
    static constexpr int HEARTBEAT_TIMEOUT_SECONDS = 30;
    static constexpr int CLEANUP_INTERVAL_SECONDS = 10;
    static constexpr int RELAY_TIMEOUT_SECONDS = 60;
};

