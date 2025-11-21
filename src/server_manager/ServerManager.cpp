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
    if (!publicIP.empty()) {
        std::cout << "Public IP: " << publicIP << std::endl;
        std::cout << "Clients should connect to: " << publicIP << ":" << m_port << std::endl;
    } else {
        std::cout << "Warning: Could not detect public IP. Server may not be accessible from the internet." << std::endl;
        std::cout << "Local IP: " << NetworkUtils::GetLocalIP() << std::endl;
    }
    
    return true;
}

void ServerManager::Run() {
    m_running = true;
    std::cout << "Server Manager running. Press Ctrl+C to stop." << std::endl;

    auto lastCleanup = std::chrono::steady_clock::now();
    const auto cleanupInterval = std::chrono::seconds(CLEANUP_INTERVAL_SECONDS);

    while (m_running) {
        // Receive messages
        char buffer[4096];
        std::string fromIP;
        uint16_t fromPort;
        
        int received = NetworkUtils::ReceiveFrom(m_socket, buffer, sizeof(buffer), fromIP, fromPort);
        
        if (received > 0) {
            if (received >= static_cast<int>(sizeof(MessageHeader))) {
                ProcessMessage(fromIP, fromPort, buffer, received);
            }
        }

        // Cleanup stale rooms periodically
        auto now = std::chrono::steady_clock::now();
        if (now - lastCleanup >= cleanupInterval) {
            CleanupStaleRooms();
            lastCleanup = now;
        }

        // Small sleep to prevent CPU spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
}

void ServerManager::SendResponse(const std::string& toIP, uint16_t toPort,
                                const void* data, size_t length) {
    NetworkUtils::SendTo(m_socket, data, length, toIP, toPort);
}

