#include "ServerManager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>

ServerManager::ServerManager(uint16_t port)
    : m_port(port)
    , m_running(false)
    , m_socket(INVALID_SOCKET_HANDLE)
    , m_randomGenerator(m_randomDevice())
    , m_codeDistribution(0, 35) // 0-9, A-Z
    , relayEnabled(true)  // Relay enabled by default
    , forcedConnectionType(::ForcedConnectionType::NONE)  // No forcing by default
{
}

ServerManager::~ServerManager() {
    Shutdown();
}

bool ServerManager::Initialize() {
    if (!NetworkUtils::Initialize()) {
        std::cerr << "Failed to initialize networking" << std::endl;
        return false;
    }

    m_socket = NetworkUtils::CreateUDPSocket();
    if (m_socket == INVALID_SOCKET_HANDLE) {
        std::cerr << "Failed to create UDP socket" << std::endl;
        NetworkUtils::Cleanup();
        return false;
    }

    if (!NetworkUtils::BindSocket(m_socket, "0.0.0.0", m_port)) {
        std::cerr << "Failed to bind socket to port " << m_port << std::endl;
        NetworkUtils::CloseSocket(m_socket);
        NetworkUtils::Cleanup();
        return false;
    }

    std::cout << "Server Manager initialized on port " << m_port << std::endl;
    
    // Detect and print public IP
    std::cout << "Detecting public IP address..." << std::endl;
    std::string publicIP = NetworkUtils::GetPublicIP();
    std::string localIP = NetworkUtils::GetLocalIP();
    
    if (!publicIP.empty()) {
        std::cout << "Public IP: " << publicIP << std::endl;
        std::cout << "Local IP: " << localIP << std::endl;
        std::cout << "Clients/Hosts should connect to: " << publicIP << ":" << m_port << std::endl;
        
        // Check if ServerManager is likely behind NAT
        if (publicIP != localIP && localIP != "127.0.0.1") {
            std::cout << "Note: Server appears to be behind a NAT/router." << std::endl;
            std::cout << "      Port forwarding may be required for external clients to connect." << std::endl;
            std::cout << "      Forward UDP port " << m_port << " to " << localIP << ":" << m_port << std::endl;
        }
    } else {
        std::cout << "Warning: Could not detect public IP. Server may not be accessible from the internet." << std::endl;
        std::cout << "Local IP: " << localIP << std::endl;
        std::cout << "Note: For external access, port forwarding may be required." << std::endl;
    }
    
    return true;
}

void ServerManager::Run() {
    m_running = true;
    std::cout << "Server Manager running. Press Ctrl+C to stop." << std::endl;

    auto lastCleanup = std::chrono::steady_clock::now();
    const auto cleanupInterval = std::chrono::seconds(CLEANUP_INTERVAL_SECONDS);

    while (m_running) {
        // Process all available messages in a batch to reduce latency
        int messagesProcessed = 0;
        const int maxMessagesPerLoop = 100; // Prevent infinite loop if messages arrive faster than we can process
        
        while (messagesProcessed < maxMessagesPerLoop) {
            char buffer[4096];
            std::string fromIP;
            uint16_t fromPort;
            
            int received = NetworkUtils::ReceiveFrom(m_socket, buffer, sizeof(buffer), fromIP, fromPort);
            
            if (received > 0) {
                if (received >= static_cast<int>(sizeof(MessageHeader))) {
                    ProcessMessage(fromIP, fromPort, buffer, received);
                    messagesProcessed++;
                }
            } else {
                // No more messages available, break out of inner loop
                break;
            }
        }

        // Cleanup stale rooms periodically
        auto now = std::chrono::steady_clock::now();
        if (now - lastCleanup >= cleanupInterval) {
            CleanupStaleRooms();
            lastCleanup = now;
        }

        // Small sleep only if no messages were processed (prevents CPU spinning)
        if (messagesProcessed == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void ServerManager::Shutdown() {
    m_running = false;
    
    if (m_socket != INVALID_SOCKET_HANDLE) {
        NetworkUtils::CloseSocket(m_socket);
        m_socket = INVALID_SOCKET_HANDLE;
    }
    
    NetworkUtils::Cleanup();
    std::cout << "Server Manager shut down." << std::endl;
}

void ServerManager::ProcessMessage(const std::string& fromIP, uint16_t fromPort,
                                   const void* data, size_t length) {
    if (length < sizeof(MessageHeader)) {
        return;
    }

    const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data);

    switch (header->type) {
        case MessageType::HOST_REGISTER:
            if (length >= sizeof(HostRegisterMessage)) {
                HandleHostRegister(fromIP, fromPort, 
                                  *reinterpret_cast<const HostRegisterMessage*>(data));
            }
            break;

        case MessageType::HOST_HEARTBEAT:
            HandleHostHeartbeat(fromIP, fromPort);
            break;

        case MessageType::HOST_UPDATE:
            HandleHostUpdate(fromIP, fromPort, data, length);
            break;

        case MessageType::CLIENT_LOOKUP:
            if (length >= sizeof(ClientLookupMessage)) {
                HandleClientLookup(fromIP, fromPort,
                                  *reinterpret_cast<const ClientLookupMessage*>(data));
            }
            break;

        case MessageType::NAT_PUNCHTHROUGH_REQUEST:
            if (length >= sizeof(NATPunchthroughRequest)) {
                HandleNATPunchthroughRequest(fromIP, fromPort,
                                            *reinterpret_cast<const NATPunchthroughRequest*>(data));
            }
            break;

        case MessageType::RELAY_REQUEST:
            if (length >= sizeof(RelayRequest)) {
                HandleRelayRequest(fromIP, fromPort,
                                  *reinterpret_cast<const RelayRequest*>(data));
            }
            break;

        case MessageType::RELAY_DATA:
            HandleRelayData(fromIP, fromPort, data, length);
            break;

        case MessageType::PATH_TEST_REQUEST:
            if (length >= sizeof(PathTestRequest)) {
                HandlePathTestRequest(fromIP, fromPort,
                                     *reinterpret_cast<const PathTestRequest*>(data));
            }
            break;

        default:
            std::cout << "Unknown message type: " << static_cast<int>(header->type) << std::endl;
            break;
    }
}

void ServerManager::HandleHostRegister(const std::string& fromIP, uint16_t fromPort,
                                      const HostRegisterMessage& msg) {
    std::lock_guard<std::mutex> lock(m_roomsMutex);

    std::string hostKey = fromIP + ":" + std::to_string(msg.hostPort);
    
    // Check if host already has a room
    auto it = m_hostToRoom.find(hostKey);
    if (it != m_hostToRoom.end()) {
        // Host already registered, send existing room code
        std::string roomCode = it->second;
        auto roomIt = m_rooms.find(roomCode);
        if (roomIt != m_rooms.end()) {
            // Update host IPs in case they changed
            roomIt->second.hostIP = std::string(msg.hostIP);      // Local IP
            roomIt->second.hostPublicIP = fromIP;                 // Public IP (from NAT)
            roomIt->second.hostPublicPort = fromPort;             // Public port (from NAT)
            
            RegisterResponse response;
            response.header.type = MessageType::RESPONSE_REGISTER;
            memset(response.header.reserved, 0, sizeof(response.header.reserved));
            strncpy(response.roomCode, roomCode.c_str(), sizeof(response.roomCode) - 1);
            response.roomCode[sizeof(response.roomCode) - 1] = '\0';
            response.hostPort = roomIt->second.hostPort;
            response.reserved[0] = 0;
            
            SendResponse(fromIP, fromPort, &response, sizeof(response));
            std::cout << "Host " << hostKey << " re-registered, room code: " << roomCode 
                     << " (Public: " << fromIP << ":" << fromPort << ")" << std::endl;
            return;
        }
    }

    // Generate new room code
    std::string roomCode = GenerateRoomCode();
    
    RoomInfo room;
    room.roomCode = roomCode;
    // Store both local and public IPs/ports
    room.hostIP = std::string(msg.hostIP);    // Local IP from message
    room.hostPort = msg.hostPort;             // Local port from message
    room.hostPublicIP = fromIP;               // Public IP detected from NAT (where the packet came from)
    room.hostPublicPort = fromPort;           // Public port detected from NAT
    room.connectedPlayers = 0;
    room.lastPing = std::chrono::steady_clock::now();
    room.createdAt = std::chrono::steady_clock::now();

    m_rooms[roomCode] = room;
    m_hostToRoom[hostKey] = roomCode;

    // Send response with room code
    RegisterResponse response;
    response.header.type = MessageType::RESPONSE_REGISTER;
    memset(response.header.reserved, 0, sizeof(response.header.reserved));
    strncpy(response.roomCode, roomCode.c_str(), sizeof(response.roomCode) - 1);
    response.roomCode[sizeof(response.roomCode) - 1] = '\0';
    response.hostPort = room.hostPort;
    response.reserved[0] = 0;

    SendResponse(fromIP, fromPort, &response, sizeof(response));
    std::cout << "Host " << hostKey << " registered with room code: " << roomCode 
             << " (Local: " << msg.hostIP << ":" << msg.hostPort 
             << ", Public: " << fromIP << ":" << fromPort << ")" << std::endl;
}

void ServerManager::HandleHostHeartbeat(const std::string& fromIP, uint16_t fromPort) {
    std::lock_guard<std::mutex> lock(m_roomsMutex);

    // Try to find room by public IP:port first
    // The hostKey format is "publicIP:localPort" for backward compatibility
    // But we also need to update the public IP/port on heartbeat in case NAT changed
    for (auto& [hostKey, roomCode] : m_hostToRoom) {
        auto roomIt = m_rooms.find(roomCode);
        if (roomIt != m_rooms.end() && 
            (roomIt->second.hostPublicIP == fromIP || 
             roomIt->second.hostIP == fromIP)) {
            // Update public IP/port in case NAT changed them
            roomIt->second.hostPublicIP = fromIP;
            roomIt->second.hostPublicPort = fromPort;
            roomIt->second.lastPing = std::chrono::steady_clock::now();
            return;
        }
    }
}

void ServerManager::HandleHostUpdate(const std::string& fromIP, uint16_t fromPort,
                                    const void* data, size_t length) {
    if (length < sizeof(HostUpdateMessage)) {
        return;
    }

    const HostUpdateMessage* msg = reinterpret_cast<const HostUpdateMessage*>(data);
    uint16_t playerCount = msg->playerCount;

    std::lock_guard<std::mutex> lock(m_roomsMutex);

    // Find room by public IP (since that's what we see from the packet)
    for (auto& [hostKey, roomCode] : m_hostToRoom) {
        auto roomIt = m_rooms.find(roomCode);
        if (roomIt != m_rooms.end() && 
            (roomIt->second.hostPublicIP == fromIP || 
             roomIt->second.hostIP == fromIP)) {
            // Update public IP/port in case NAT changed them
            roomIt->second.hostPublicIP = fromIP;
            roomIt->second.hostPublicPort = fromPort;
            roomIt->second.lastPing = std::chrono::steady_clock::now();
            roomIt->second.connectedPlayers = playerCount;
            roomIt->second.playerIPs.clear();

            // Parse player IPs from message
            const char* ipData = reinterpret_cast<const char*>(data) + sizeof(HostUpdateMessage);
            size_t offset = 0;
            
            for (uint16_t i = 0; i < playerCount && offset < (length - sizeof(HostUpdateMessage)); ++i) {
                std::string playerIP(ipData + offset);
                if (!playerIP.empty()) {
                    roomIt->second.playerIPs.push_back(playerIP);
                }
                offset += playerIP.length() + 1; // +1 for null terminator
            }

            std::cout << "Host " << roomCode << " updated: " << playerCount 
                     << " players (Public: " << fromIP << ":" << fromPort << ")" << std::endl;
            return;
        }
    }
}

void ServerManager::HandleClientLookup(const std::string& fromIP, uint16_t fromPort,
                                      const ClientLookupMessage& msg) {
    std::lock_guard<std::mutex> lock(m_roomsMutex);

    std::string roomCode(msg.roomCode);
    
    auto it = m_rooms.find(roomCode);
    if (it != m_rooms.end()) {
        const RoomInfo& room = it->second;
        
        // Build response with room info and player IPs
        // Use public IP/port for NAT traversal (clients should connect to the public IP)
        std::vector<char> responseBuffer(sizeof(RoomInfoResponse) + 
                                        room.playerIPs.size() * 16); // Max IP length
        
        RoomInfoResponse* response = reinterpret_cast<RoomInfoResponse*>(responseBuffer.data());
        response->header.type = MessageType::RESPONSE_ROOM_INFO;
        memset(response->header.reserved, 0, sizeof(response->header.reserved));
        response->hostPort = room.hostPort;                    // Local port (for reference)
        response->hostPublicPort = room.hostPublicPort;        // Public port (use this to connect)
        response->playerCount = static_cast<uint16_t>(room.playerIPs.size());
        response->forcedConnectionType = static_cast<uint8_t>(forcedConnectionType);
        response->relayEnabled = relayEnabled ? 1 : 0;
        memset(response->reserved, 0, sizeof(response->reserved));
        
        // Store both local and public IPs
        strncpy(response->hostIP, room.hostIP.c_str(), sizeof(response->hostIP) - 1);
        response->hostIP[sizeof(response->hostIP) - 1] = '\0';
        
        // Public IP is what clients should use for NAT traversal
        strncpy(response->hostPublicIP, room.hostPublicIP.c_str(), sizeof(response->hostPublicIP) - 1);
        response->hostPublicIP[sizeof(response->hostPublicIP) - 1] = '\0';

        // Append player IPs
        char* ipData = responseBuffer.data() + sizeof(RoomInfoResponse);
        for (const auto& playerIP : room.playerIPs) {
            strncpy(ipData, playerIP.c_str(), 15);
            ipData[15] = '\0';
            ipData += 16;
        }

        SendResponse(fromIP, fromPort, responseBuffer.data(), responseBuffer.size());
        std::cout << "Client " << fromIP << " looked up room: " << roomCode 
                 << " (Host Public: " << room.hostPublicIP << ":" << room.hostPublicPort << ")" << std::endl;
        
        // Log forced connection type if set
        if (forcedConnectionType != ::ForcedConnectionType::NONE) {
            std::string typeStr;
            switch (forcedConnectionType) {
                case ::ForcedConnectionType::DIRECT_ONLY: typeStr = "DIRECT_ONLY"; break;
                case ::ForcedConnectionType::NAT_ONLY: typeStr = "NAT_ONLY"; break;
                case ::ForcedConnectionType::RELAY_ONLY: typeStr = "RELAY_ONLY"; break;
                default: break;
            }
            std::cout << "  Forced connection type: " << typeStr << std::endl;
        }
    } else {
        ErrorResponse response;
        response.header.type = MessageType::RESPONSE_ERROR;
        memset(response.header.reserved, 0, sizeof(response.header.reserved));
        strncpy(response.errorMessage, "Room not found", sizeof(response.errorMessage) - 1);
        response.errorMessage[sizeof(response.errorMessage) - 1] = '\0';

        SendResponse(fromIP, fromPort, &response, sizeof(response));
        std::cout << "Client " << fromIP << " looked up invalid room: " << roomCode << std::endl;
    }
}

std::string ServerManager::GenerateRoomCode() {
    const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string code;
    code.reserve(ROOM_CODE_LENGTH);

    // Keep generating until we get a unique code
    do {
        code.clear();
        for (int i = 0; i < ROOM_CODE_LENGTH; ++i) {
            code += chars[m_codeDistribution(m_randomGenerator)];
        }
    } while (m_rooms.find(code) != m_rooms.end());

    return code;
}

void ServerManager::CleanupStaleRooms() {
    std::lock_guard<std::mutex> lock(m_roomsMutex);

    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(HEARTBEAT_TIMEOUT_SECONDS);

    std::vector<std::string> roomsToRemove;
    
    for (auto& pair : m_rooms) {
        if (now - pair.second.lastPing > timeout) {
            roomsToRemove.push_back(pair.first);
        }
    }

    for (const auto& roomCode : roomsToRemove) {
        auto it = m_rooms.find(roomCode);
        if (it != m_rooms.end()) {
            std::string hostKey = it->second.hostIP + ":" + std::to_string(it->second.hostPort);
            m_hostToRoom.erase(hostKey);
            m_rooms.erase(it);
            std::cout << "Removed stale room: " << roomCode << std::endl;
        }
    }

    // Cleanup stale relay connections
    {
        std::lock_guard<std::mutex> relayLock(relaysMutex);
        auto relayTimeout = std::chrono::seconds(RELAY_TIMEOUT_SECONDS);
        for (auto& [roomCode, relays] : activeRelays) {
            relays.erase(
                std::remove_if(relays.begin(), relays.end(),
                    [&](const RelayConnection& relay) {
                        return (now - relay.lastActivity) > relayTimeout;
                    }),
                relays.end());
        }
        // Remove empty relay entries
        for (auto it = activeRelays.begin(); it != activeRelays.end();) {
            if (it->second.empty()) {
                it = activeRelays.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void ServerManager::SendResponse(const std::string& toIP, uint16_t toPort,
                                const void* data, size_t length) {
    NetworkUtils::SendTo(m_socket, data, length, toIP, toPort);
}

void ServerManager::HandleNATPunchthroughRequest(const std::string& fromIP, uint16_t fromPort,
                                                const NATPunchthroughRequest& msg) {
    // Check if NAT punchthrough is allowed
    if (forcedConnectionType == ::ForcedConnectionType::DIRECT_ONLY || 
        forcedConnectionType == ::ForcedConnectionType::RELAY_ONLY) {
        ErrorResponse response;
        response.header.type = MessageType::RESPONSE_ERROR;
        memset(response.header.reserved, 0, sizeof(response.header.reserved));
        strncpy(response.errorMessage, "NAT punchthrough not allowed (forced connection type)", sizeof(response.errorMessage) - 1);
        response.errorMessage[sizeof(response.errorMessage) - 1] = '\0';
        SendResponse(fromIP, fromPort, &response, sizeof(response));
        std::cout << "NAT punchthrough request denied for room " << msg.roomCode 
                 << " (forced connection type)" << std::endl;
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_roomsMutex);

    std::string roomCode(msg.roomCode);
    auto it = m_rooms.find(roomCode);
    if (it == m_rooms.end()) {
        ErrorResponse response;
        response.header.type = MessageType::RESPONSE_ERROR;
        memset(response.header.reserved, 0, sizeof(response.header.reserved));
        strncpy(response.errorMessage, "Room not found", sizeof(response.errorMessage) - 1);
        response.errorMessage[sizeof(response.errorMessage) - 1] = '\0';
        SendResponse(fromIP, fromPort, &response, sizeof(response));
        return;
    }

    const RoomInfo& room = it->second;

    // Send punchthrough info to both host and client
    NATPunchthroughResponse response;
    response.header.type = MessageType::NAT_PUNCHTHROUGH_RESPONSE;
    memset(response.header.reserved, 0, sizeof(response.header.reserved));
    strncpy(response.roomCode, roomCode.c_str(), sizeof(response.roomCode) - 1);
    response.roomCode[sizeof(response.roomCode) - 1] = '\0';

    // Host info
    strncpy(response.hostPublicIP, room.hostPublicIP.c_str(), sizeof(response.hostPublicIP) - 1);
    response.hostPublicIP[sizeof(response.hostPublicIP) - 1] = '\0';
    response.hostPublicPort = room.hostPublicPort;
    strncpy(response.hostLocalIP, room.hostIP.c_str(), sizeof(response.hostLocalIP) - 1);
    response.hostLocalIP[sizeof(response.hostLocalIP) - 1] = '\0';
    response.hostLocalPort = room.hostPort;

    // Client info (from request and detected public IP)
    strncpy(response.clientPublicIP, fromIP.c_str(), sizeof(response.clientPublicIP) - 1);
    response.clientPublicIP[sizeof(response.clientPublicIP) - 1] = '\0';
    response.clientPublicPort = fromPort;
    strncpy(response.clientLocalIP, msg.clientLocalIP, sizeof(response.clientLocalIP) - 1);
    response.clientLocalIP[sizeof(response.clientLocalIP) - 1] = '\0';
    response.clientLocalPort = msg.clientLocalPort;

    // Send to client
    SendResponse(fromIP, fromPort, &response, sizeof(response));

    // Send to host
    SendResponse(room.hostPublicIP, room.hostPublicPort, &response, sizeof(response));

    std::cout << "NAT punchthrough coordination for room " << roomCode 
             << " (Host: " << room.hostPublicIP << ":" << room.hostPublicPort
             << ", Client: " << fromIP << ":" << fromPort << ")" << std::endl;
}

void ServerManager::HandleRelayRequest(const std::string& fromIP, uint16_t fromPort,
                                      const RelayRequest& msg) {
    std::lock_guard<std::mutex> lock(m_roomsMutex);

    std::string roomCode(msg.roomCode);
    auto it = m_rooms.find(roomCode);
    if (it == m_rooms.end()) {
        ErrorResponse response;
        response.header.type = MessageType::RESPONSE_ERROR;
        memset(response.header.reserved, 0, sizeof(response.header.reserved));
        strncpy(response.errorMessage, "Room not found", sizeof(response.errorMessage) - 1);
        response.errorMessage[sizeof(response.errorMessage) - 1] = '\0';
        SendResponse(fromIP, fromPort, &response, sizeof(response));
        return;
    }

    const RoomInfo& room = it->second;

    RelayResponse response;
    response.header.type = MessageType::RELAY_RESPONSE;
    memset(response.header.reserved, 0, sizeof(response.header.reserved));
    strncpy(response.roomCode, roomCode.c_str(), sizeof(response.roomCode) - 1);
    response.roomCode[sizeof(response.roomCode) - 1] = '\0';

    // Check if relay is forced or disabled
    bool shouldAccept = false;
    std::string declineReason;
    
    if (forcedConnectionType == ::ForcedConnectionType::RELAY_ONLY) {
        // Force relay - accept if enabled
        shouldAccept = relayEnabled;
        if (!relayEnabled) {
            declineReason = "Relay disabled by server (forced RELAY_ONLY but relay disabled)";
        }
    } else if (forcedConnectionType == ::ForcedConnectionType::DIRECT_ONLY || 
               forcedConnectionType == ::ForcedConnectionType::NAT_ONLY) {
        // Relay not allowed due to forced connection type
        shouldAccept = false;
        declineReason = "Relay not allowed (forced connection type)";
    } else {
        // Normal mode - check relay enabled
        shouldAccept = relayEnabled;
        if (!relayEnabled) {
            declineReason = "Relay disabled by server";
        }
    }

    if (!shouldAccept) {
        // Relay declined
        response.accepted = 0;
        response.relayPort = 0;
        SendResponse(fromIP, fromPort, &response, sizeof(response));

        // Notify host that relay was declined
        RelayDecline decline;
        decline.header.type = MessageType::RELAY_DECLINE;
        memset(decline.header.reserved, 0, sizeof(decline.header.reserved));
        strncpy(decline.roomCode, roomCode.c_str(), sizeof(decline.roomCode) - 1);
        decline.roomCode[sizeof(decline.roomCode) - 1] = '\0';
        strncpy(decline.clientIP, fromIP.c_str(), sizeof(decline.clientIP) - 1);
        decline.clientIP[sizeof(decline.clientIP) - 1] = '\0';
        decline.clientPort = fromPort;
        strncpy(decline.reason, declineReason.c_str(), sizeof(decline.reason) - 1);
        decline.reason[sizeof(decline.reason) - 1] = '\0';
        SendResponse(room.hostPublicIP, room.hostPublicPort, &decline, sizeof(decline));

        std::cout << "Relay request declined for room " << roomCode 
                 << " - " << declineReason << std::endl;
        return;
    }

    // Accept relay connection
    response.accepted = 1;
    response.relayPort = m_port;  // Use same port for relay
    SendResponse(fromIP, fromPort, &response, sizeof(response));

    // Register relay connection
    {
        std::lock_guard<std::mutex> relayLock(relaysMutex);
        RelayConnection relayConn;
        relayConn.clientIP = fromIP;
        relayConn.clientPort = fromPort;
        relayConn.hostIP = room.hostPublicIP;
        relayConn.hostPort = room.hostPublicPort;
        relayConn.lastActivity = std::chrono::steady_clock::now();
        activeRelays[roomCode].push_back(relayConn);
    }

    std::cout << "Relay connection established for room " << roomCode 
             << " (Client: " << fromIP << ":" << fromPort
             << " -> Host: " << room.hostPublicIP << ":" << room.hostPublicPort << ")" << std::endl;
}

void ServerManager::HandleRelayData(const std::string& fromIP, uint16_t fromPort,
                                   const void* data, size_t length) {
    if (length < sizeof(RelayDataHeader)) {
        return;
    }

    const RelayDataHeader* header = reinterpret_cast<const RelayDataHeader*>(data);
    std::string roomCode(header->roomCode);

    // Get room info and relay connection info with minimal lock time
    std::string hostIP;
    uint16_t hostPort = 0;
    std::string toIP;
    uint16_t toPort = 0;
    bool isValid = false;
    
    {
        std::lock_guard<std::mutex> lock(m_roomsMutex);
        auto roomIt = m_rooms.find(roomCode);
        if (roomIt == m_rooms.end()) {
            return;
        }
        const RoomInfo& room = roomIt->second;
        hostIP = room.hostPublicIP;
        hostPort = room.hostPublicPort;
    }

    {
        std::lock_guard<std::mutex> relayLock(relaysMutex);

        // Verify this is a valid relay connection
        auto it = activeRelays.find(roomCode);
        if (it == activeRelays.end()) {
            return;
        }

        // Determine destination based on sender
        // The client registered the relay connection via RELAY_REQUEST, so we know:
        // - relay.clientIP/clientPort = the client's ServerManager socket (exact IP/port from RELAY_REQUEST)
        // - relay.hostIP/hostPort = the host's public IP/port (from room registration)
        // Strategy: Match client first (exact match), then assume host if it doesn't match
        
        // For each relay connection in this room, determine destination
        // Since there's typically one relay connection per room, we can use the first one
        for (const auto& relay : it->second) {
            // First, check if sender is the client (exact match with stored client IP/port)
            bool isClientSender = (relay.clientIP == fromIP && relay.clientPort == fromPort);
            
            if (isClientSender) {
                // Client sent data, forward to host
                toIP = relay.hostIP;
                toPort = relay.hostPort;
                isValid = true;
                break;
            } else {
                // Not the client, so must be the host
                // Forward to client (use stored client IP/port from relay registration)
                toIP = relay.clientIP;
                toPort = relay.clientPort;
                isValid = true;
                break;
            }
        }
        
        // Update last activity (still within lock)
        if (isValid) {
            for (auto& relay : it->second) {
                if ((relay.clientIP == fromIP && relay.clientPort == fromPort) ||
                    (relay.hostIP == fromIP && relay.hostPort == fromPort)) {
                    relay.lastActivity = std::chrono::steady_clock::now();
                    break;
                }
            }
        }
    }

    if (!isValid) {
        std::cerr << "ServerManager: No relay connection found for room " << roomCode << std::endl;
        return;
    }

    // Forward the data with relay header (so receiver knows it's relay data)
    // The receiver (ConnectionManager) will strip the header
    NetworkUtils::SendTo(m_socket, data, length, toIP, toPort);
}

void ServerManager::HandlePathTestRequest(const std::string& fromIP, uint16_t fromPort,
                                         const PathTestRequest& msg) {
    std::lock_guard<std::mutex> lock(m_roomsMutex);

    std::string roomCode(msg.roomCode);
    auto it = m_rooms.find(roomCode);
    if (it == m_rooms.end()) {
        return;
    }

    // Calculate latency
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    uint32_t latencyMs = static_cast<uint32_t>(timestamp - msg.timestamp);

    PathTestResponse response;
    response.header.type = MessageType::PATH_TEST_RESPONSE;
    memset(response.header.reserved, 0, sizeof(response.header.reserved));
    strncpy(response.roomCode, roomCode.c_str(), sizeof(response.roomCode) - 1);
    response.roomCode[sizeof(response.roomCode) - 1] = '\0';
    response.timestamp = msg.timestamp;
    response.latencyMs = latencyMs;

    SendResponse(fromIP, fromPort, &response, sizeof(response));
}

