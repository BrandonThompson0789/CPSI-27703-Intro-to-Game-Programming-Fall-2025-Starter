#include "HostManager.h"
#include "Engine.h"
#include "server_manager/ServerManager.h"
#include "components/BodyComponent.h"
#include "components/SpriteComponent.h"
#include "components/SoundComponent.h"
#include "components/ViewGrabComponent.h"
#include "components/InputComponent.h"
#include "BackgroundManager.h"
#include "CompressionUtils.h"
#include "PlayerManager.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <thread>
#include <atomic>
#include <fstream>
#include <limits>

namespace {
constexpr uint16_t kDefaultHostPort = 8889;
constexpr const char* kDefaultServerManagerIP = "127.0.0.1";
constexpr uint16_t kDefaultServerManagerPort = 8888;
constexpr uint32_t kDefaultSyncIntervalMs = 20;
constexpr uint32_t kDefaultHeartbeatSeconds = 5;
constexpr const char* kServerDataPath = "assets/serverData.json";
}

HostManager::HostManager(Engine* engine)
    : engine(engine)
    , serverManagerSocket(INVALID_SOCKET_HANDLE)
    , hostPort(kDefaultHostPort)
    , serverManagerIP(kDefaultServerManagerIP)
    , serverManagerPort(kDefaultServerManagerPort)
    , roomCode("")
    , isHosting(false)
    , nextObjectId(1)
    , syncInterval(std::chrono::milliseconds(kDefaultSyncIntervalMs))
    , serverManagerHeartbeatInterval(std::chrono::seconds(kDefaultHeartbeatSeconds))
    , bytesSent(0)
    , bytesReceived(0)
{
    lastSyncTime = std::chrono::steady_clock::now();
    lastServerManagerHeartbeat = std::chrono::steady_clock::now();
    lastBandwidthLogTime = std::chrono::steady_clock::now();

    serverDataConfig.hostPort = kDefaultHostPort;
    serverDataConfig.serverManagerIP = kDefaultServerManagerIP;
    serverDataConfig.serverManagerPort = kDefaultServerManagerPort;
    serverDataConfig.syncIntervalMs = kDefaultSyncIntervalMs;
    serverDataConfig.heartbeatSeconds = kDefaultHeartbeatSeconds;

    LoadServerDataConfig();

    hostPort = serverDataConfig.hostPort;
    serverManagerIP = serverDataConfig.serverManagerIP;
    serverManagerPort = serverDataConfig.serverManagerPort;
    syncInterval = std::chrono::milliseconds(serverDataConfig.syncIntervalMs);
    serverManagerHeartbeatInterval = std::chrono::seconds(serverDataConfig.heartbeatSeconds);
}

HostManager::~HostManager() {
    Shutdown();
}

bool HostManager::Initialize(uint16_t hostPortParam, const std::string& serverManagerIPParam, uint16_t serverManagerPortParam) {
    uint16_t resolvedHostPort = hostPortParam;
    std::string resolvedServerManagerIP = serverManagerIPParam;
    uint16_t resolvedServerManagerPort = serverManagerPortParam;

    if (serverDataConfig.loaded) {
        if (hostPortParam == kDefaultHostPort) {
            resolvedHostPort = serverDataConfig.hostPort;
        }
        if (serverManagerIPParam == kDefaultServerManagerIP) {
            resolvedServerManagerIP = serverDataConfig.serverManagerIP;
        }
        if (serverManagerPortParam == kDefaultServerManagerPort) {
            resolvedServerManagerPort = serverDataConfig.serverManagerPort;
        }
    }

    hostPort = resolvedHostPort;
    serverManagerIP = resolvedServerManagerIP;
    serverManagerPort = resolvedServerManagerPort;

    // Initialize ConnectionManager (ENet) for game networking
    if (!connectionManager.Initialize()) {
        std::cerr << "HostManager: Failed to initialize ConnectionManager" << std::endl;
        return false;
    }

    // Start hosting with ConnectionManager
    if (!connectionManager.StartHost(hostPort)) {
        std::cerr << "HostManager: Failed to start host on port " << hostPort << std::endl;
        return false;
    }

    // Initialize NetworkUtils for ServerManager communication
    if (!NetworkUtils::Initialize()) {
        std::cerr << "HostManager: Failed to initialize NetworkUtils for ServerManager" << std::endl;
        connectionManager.Cleanup();
        return false;
    }

    // Create socket for ServerManager communication
    serverManagerSocket = NetworkUtils::CreateUDPSocket();
    if (serverManagerSocket == INVALID_SOCKET_HANDLE) {
        std::cerr << "HostManager: Failed to create ServerManager socket" << std::endl;
        NetworkUtils::Cleanup();
        connectionManager.Cleanup();
        return false;
    }

    // Register with Server Manager
    if (!RegisterWithServerManager()) {
        std::cerr << "HostManager: Failed to register with Server Manager" << std::endl;
        NetworkUtils::CloseSocket(serverManagerSocket);
        NetworkUtils::Cleanup();
        connectionManager.Cleanup();
        return false;
    }

    // Set ServerManager socket in ConnectionManager so it can receive relay data
    connectionManager.SetServerManagerSocket(serverManagerSocket, serverManagerIP, serverManagerPort);

    isHosting = true;
    std::cout << "HostManager: Started hosting on port " << hostPort << " with room code: " << roomCode << std::endl;
    return true;
}

void HostManager::Update(float deltaTime) {
    if (!isHosting) {
        return;
    }

    // Update ConnectionManager (processes ENet events)
    connectionManager.Update(deltaTime);

    // Process incoming messages from clients
    ProcessIncomingMessages();
    
    // Process ServerManager messages (relay decline, NAT punchthrough, etc.)
    ProcessServerManagerMessages();

    // Log bandwidth statistics once per second
    auto now = std::chrono::steady_clock::now();
    if (now - lastBandwidthLogTime >= std::chrono::seconds(1)) {
        uint64_t sent = bytesSent.exchange(0);
        uint64_t received = bytesReceived.exchange(0);
        double sentKB = static_cast<double>(sent) / 1024.0;
        double receivedKB = static_cast<double>(received) / 1024.0;
        std::cout << "HostManager: Bandwidth (last second) - Sent: " << sentKB << " KB, Received: " << receivedKB << " KB" << std::endl;
        lastBandwidthLogTime = now;
    }

    // Send heartbeat to Server Manager periodically
    if (now - lastServerManagerHeartbeat >= serverManagerHeartbeatInterval) {
        SendHeartbeatToServerManager();
        UpdateServerManagerWithClients();
        lastServerManagerHeartbeat = now;
    }

    // Send object updates periodically (every 20ms)
    if (now - lastSyncTime >= syncInterval) {
        SendObjectUpdates();
        lastSyncTime = now;
    }

    // Cleanup disconnected clients
    CleanupDisconnectedClients();

    // Cleanup object IDs for destroyed objects
    CleanupObjectIds();
}

void HostManager::Shutdown() {
    if (!isHosting) {
        return;
    }

    // Notify all clients that the session has ended before shutting down
    NotifyClientsSessionEnded();
    
    // Give clients a moment to receive the message
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    isHosting = false;

    // Stop hosting with ConnectionManager
    connectionManager.StopHost();

    // Close ServerManager socket
    if (serverManagerSocket != INVALID_SOCKET_HANDLE) {
        NetworkUtils::CloseSocket(serverManagerSocket);
        serverManagerSocket = INVALID_SOCKET_HANDLE;
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.clear();
    }

    {
        std::lock_guard<std::mutex> lock(objectIdsMutex);
        objectToId.clear();
        idToObject.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(stateTrackingMutex);
        lastSentState.clear();
    }

    NetworkUtils::Cleanup();
    connectionManager.Cleanup();
    std::cout << "HostManager: Shutdown complete" << std::endl;
}

bool HostManager::RegisterWithServerManager() {
    HostRegisterMessage msg;
    msg.header.type = MessageType::HOST_REGISTER;
    memset(msg.header.reserved, 0, sizeof(msg.header.reserved));
    msg.hostPort = hostPort;
    msg.reserved[0] = 0;
    
    // Get local network IP (not localhost)
    std::string localIP = NetworkUtils::GetLocalIP();
    strncpy(msg.hostIP, localIP.c_str(), sizeof(msg.hostIP) - 1);
    msg.hostIP[sizeof(msg.hostIP) - 1] = '\0';

    // Send registration message (use ServerManager socket)
    NetworkUtils::SendTo(serverManagerSocket, &msg, sizeof(msg), serverManagerIP, serverManagerPort);

    // Wait for response (with timeout)
    auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(5);

    char buffer[4096];
    std::string fromIP;
    uint16_t fromPort;

    while (std::chrono::steady_clock::now() - startTime < timeout) {
        int received = NetworkUtils::ReceiveFrom(serverManagerSocket, buffer, sizeof(buffer), fromIP, fromPort);
        
        if (received >= static_cast<int>(sizeof(RegisterResponse))) {
            const RegisterResponse* response = reinterpret_cast<const RegisterResponse*>(buffer);
            if (response->header.type == MessageType::RESPONSE_REGISTER) {
                roomCode = std::string(response->roomCode);
                std::cout << "HostManager: Registered with Server Manager, room code: " << roomCode << std::endl;
                return true;
            }
        }

        // Small sleep to prevent CPU spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cerr << "HostManager: Timeout waiting for Server Manager response" << std::endl;
    return false;
}

void HostManager::SendHeartbeatToServerManager() {
    HostHeartbeatMessage msg;
    msg.header.type = MessageType::HOST_HEARTBEAT;
    memset(msg.header.reserved, 0, sizeof(msg.header.reserved));

    NetworkUtils::SendTo(serverManagerSocket, &msg, sizeof(msg), serverManagerIP, serverManagerPort);
}

void HostManager::UpdateServerManagerWithClients() {
    std::lock_guard<std::mutex> lock(clientsMutex);
    
    // Build message with client IPs
    std::vector<char> buffer(sizeof(HostUpdateMessage) + clients.size() * 16);  // Max IP length
    HostUpdateMessage* msg = reinterpret_cast<HostUpdateMessage*>(buffer.data());
    msg->header.type = MessageType::HOST_UPDATE;
    memset(msg->header.reserved, 0, sizeof(msg->header.reserved));
    msg->playerCount = static_cast<uint16_t>(clients.size());
    msg->reserved = 0;

    // Append client IPs
    char* ipData = buffer.data() + sizeof(HostUpdateMessage);
    for (const auto& [key, client] : clients) {
        if (client.connected) {
            std::string clientKey = client.ip + ":" + std::to_string(client.port);
            strncpy(ipData, clientKey.c_str(), 15);
            ipData[15] = '\0';
            ipData += 16;
        }
    }

    NetworkUtils::SendTo(serverManagerSocket, buffer.data(), buffer.size(), serverManagerIP, serverManagerPort);
}

void HostManager::LoadServerDataConfig() {
    std::ifstream configStream(kServerDataPath);
    if (!configStream.is_open()) {
        std::cerr << "HostManager: Could not open " << kServerDataPath << ", using default networking parameters." << std::endl;
        return;
    }

    try {
        nlohmann::json configJson;
        configStream >> configJson;

        if (configJson.contains("hostPort") && configJson["hostPort"].is_number_unsigned()) {
            uint32_t portValue = configJson["hostPort"].get<uint32_t>();
            if (portValue > 0 && portValue <= std::numeric_limits<uint16_t>::max()) {
                serverDataConfig.hostPort = static_cast<uint16_t>(portValue);
            }
        }

        if (configJson.contains("serverManagerIP") && configJson["serverManagerIP"].is_string()) {
            serverDataConfig.serverManagerIP = configJson["serverManagerIP"].get<std::string>();
        }

        if (configJson.contains("serverManagerPort") && configJson["serverManagerPort"].is_number_unsigned()) {
            uint32_t portValue = configJson["serverManagerPort"].get<uint32_t>();
            if (portValue > 0 && portValue <= std::numeric_limits<uint16_t>::max()) {
                serverDataConfig.serverManagerPort = static_cast<uint16_t>(portValue);
            }
        }

        if (configJson.contains("syncIntervalMs") && configJson["syncIntervalMs"].is_number_unsigned()) {
            serverDataConfig.syncIntervalMs = configJson["syncIntervalMs"].get<uint32_t>();
        }

        if (configJson.contains("serverManagerHeartbeatSeconds") && configJson["serverManagerHeartbeatSeconds"].is_number_unsigned()) {
            serverDataConfig.heartbeatSeconds = configJson["serverManagerHeartbeatSeconds"].get<uint32_t>();
        }

        serverDataConfig.loaded = true;
        std::cout << "HostManager: Loaded server data config from " << kServerDataPath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "HostManager: Failed to parse " << kServerDataPath << " (" << e.what() << "), using defaults." << std::endl;
    }
}

void HostManager::ProcessIncomingMessages() {
    char buffer[4096];
    std::string fromPeerIdentifier;
    size_t received;

    while (true) {
        if (!connectionManager.Receive(buffer, sizeof(buffer), received, fromPeerIdentifier)) {
            break;
        }

        // Track received bytes
        bytesReceived.fetch_add(received);

        if (received < sizeof(HostMessageHeader)) {
            continue;
        }

        const HostMessageHeader* header = reinterpret_cast<const HostMessageHeader*>(buffer);

        // Parse IP and port from peer identifier (format: "IP:PORT" or "RELAY:ROOMCODE")
        std::string fromIP;
        uint16_t fromPort = 0;
        // Check if it's a relay connection first
        if (fromPeerIdentifier.find("RELAY:") == 0) {
            // Relay mode - use identifier as-is, port is 0
            fromIP = fromPeerIdentifier;
            fromPort = 0;
        } else {
            size_t colonPos = fromPeerIdentifier.find(':');
            if (colonPos != std::string::npos) {
                fromIP = fromPeerIdentifier.substr(0, colonPos);
                fromPort = static_cast<uint16_t>(std::stoi(fromPeerIdentifier.substr(colonPos + 1)));
            } else {
                fromIP = fromPeerIdentifier;  // Fallback
            }
        }

        switch (header->type) {
            case HostMessageType::CLIENT_CONNECT:
                HandleClientConnect(fromIP, fromPort);
                break;

            case HostMessageType::CLIENT_DISCONNECT:
                HandleClientDisconnect(fromIP, fromPort);
                break;

            case HostMessageType::CLIENT_INPUT:
                if (received >= sizeof(ClientInputMessage)) {
                    HandleClientInput(fromIP, fromPort, *reinterpret_cast<const ClientInputMessage*>(buffer));
                }
                break;

            case HostMessageType::HEARTBEAT:
                HandleClientHeartbeat(fromIP, fromPort);
                break;

            default:
                // Unknown message type
                break;
        }
    }
}

void HostManager::ProcessServerManagerMessages() {
    char buffer[4096];
    std::string fromIP;
    uint16_t fromPort;
    
    // Process all pending ServerManager messages
    while (true) {
        int received = NetworkUtils::ReceiveFrom(serverManagerSocket, buffer, sizeof(buffer), fromIP, fromPort);
        if (received <= 0 || fromIP != serverManagerIP || fromPort != serverManagerPort) {
            break;
        }
        
        if (received < static_cast<int>(sizeof(MessageHeader))) {
            continue;
        }
        
        const MessageHeader* header = 
            reinterpret_cast<const MessageHeader*>(buffer);
        
        switch (header->type) {
            case MessageType::RELAY_DECLINE:
                if (received >= static_cast<int>(sizeof(RelayDecline))) {
                    const RelayDecline* decline = 
                        reinterpret_cast<const RelayDecline*>(buffer);
                    std::cout << "HostManager: Relay connection declined for client " 
                             << decline->clientIP << ":" << decline->clientPort 
                             << " - Reason: " << decline->reason << std::endl;
                    // Host can take action here (e.g., notify user, try alternative connection)
                }
                break;
                
            case MessageType::NAT_PUNCHTHROUGH_RESPONSE:
                // NAT punchthrough coordination - ConnectionManager handles this
                // But we can log it here for debugging
                if (received >= static_cast<int>(sizeof(NATPunchthroughResponse))) {
                    const NATPunchthroughResponse* response = 
                        reinterpret_cast<const NATPunchthroughResponse*>(buffer);
                    std::cout << "HostManager: NAT punchthrough coordination received for room " 
                             << response->roomCode << std::endl;
                }
                break;
                
            case MessageType::RELAY_DATA:
                // Relay data should be handled by ConnectionManager.Receive()
                // This shouldn't normally reach here, but if it does, pass it to ConnectionManager
                // The ConnectionManager.Receive() method checks for relay data
                break;
                
            default:
                // Other ServerManager messages handled elsewhere
                break;
        }
    }
}

void HostManager::HandleClientConnect(const std::string& fromIP, uint16_t fromPort) {
    // For relay connections, fromIP is "RELAY:ROOMCODE" and fromPort is 0
    // Use the identifier as-is for relay, otherwise construct IP:PORT
    std::string clientKey;
    if (fromIP.find("RELAY:") == 0) {
        clientKey = fromIP;  // Use relay identifier as-is
    } else {
        clientKey = fromIP + ":" + std::to_string(fromPort);
    }
    
    std::cout << "HostManager: Received CLIENT_CONNECT from " << clientKey << std::endl;
    
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        auto it = clients.find(clientKey);
        if (it != clients.end() && it->second.connected) {
            // Client already connected, just send initialization again
            // Only send initialization package if level is loaded and NOT level_mainmenu
            // Otherwise, wait for level to be loaded and SendInitializationPackageToAllClients will be called
            if (engine && engine->getCurrentLoadedLevel() != "level_mainmenu" && !engine->getCurrentLoadedLevel().empty()) {
                try {
                    SendInitializationPackage(fromIP, fromPort);
                } catch (const std::exception& e) {
                    std::cerr << "HostManager: Error sending initialization package: " << e.what() << std::endl;
                }
            }
            return;
        }

        // Add new client
        ClientInfo client;
        // Store the peer identifier (for relay, this is "RELAY:ROOMCODE", for direct it's IP:PORT)
        client.ip = fromIP;  // This will be "RELAY:ROOMCODE" for relay connections
        client.port = fromPort;  // This will be 0 for relay connections
        client.lastHeartbeat = std::chrono::steady_clock::now();
        client.connected = true;
        client.assignedPlayerId = 0;  // Will be assigned below
        clients[clientKey] = client;
    }

    std::cout << "HostManager: Client connected from " << clientKey << std::endl;
    
    // For relay connections, register the peer in ConnectionManager
    if (fromIP.find("RELAY:") == 0) {
        // Extract room code from "RELAY:ROOMCODE"
        std::string roomCode = fromIP.substr(6);  // Skip "RELAY:"
        connectionManager.RegisterRelayPeer(roomCode);
    }
    
    // Assign a player slot to this client
    int assignedPlayerId = AssignPlayerToClient(clientKey);
    
    // Send player assignment first (client needs this to be considered connected)
    if (assignedPlayerId > 0) {
        SendPlayerAssignment(fromIP, fromPort, assignedPlayerId);
    }
    
    // Only send initialization package if level is loaded and NOT level_mainmenu
    // Otherwise, wait for level to be loaded and SendInitializationPackageToAllClients will be called
    if (engine && engine->getCurrentLoadedLevel() != "level_mainmenu" && !engine->getCurrentLoadedLevel().empty()) {
        try {
            SendInitializationPackage(fromIP, fromPort);
        } catch (const std::exception& e) {
            std::cerr << "HostManager: Error sending initialization package: " << e.what() << std::endl;
        }
    }
}

void HostManager::HandleClientDisconnect(const std::string& fromIP, uint16_t fromPort) {
    // For relay connections, fromIP is "RELAY:ROOMCODE" and fromPort is 0
    std::string clientKey;
    if (fromIP.find("RELAY:") == 0) {
        clientKey = fromIP;  // Use relay identifier as-is
    } else {
        clientKey = fromIP + ":" + std::to_string(fromPort);
    }
    
    int assignedPlayerId = 0;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        auto it = clients.find(clientKey);
        if (it != clients.end()) {
            // Player assignment is released when client disconnects
            // (can be reassigned to another client)
            assignedPlayerId = it->second.assignedPlayerId;
            it->second.connected = false;
            it->second.assignedPlayerId = 0;
        }
    }
    
    // Unassign network ID from player in PlayerManager
    if (assignedPlayerId > 0) {
        PlayerManager::getInstance().unassignPlayer(assignedPlayerId);
    }

    std::cout << "HostManager: Client disconnected from " << clientKey << std::endl;
}

void HostManager::HandleClientInput(const std::string& fromIP, uint16_t fromPort, const ClientInputMessage& msg) {
    // Route network input to PlayerManager using the player ID from the message
    PlayerManager::getInstance().setNetworkInput(
        msg.playerId,
        msg.moveUp,
        msg.moveDown,
        msg.moveLeft,
        msg.moveRight,
        msg.actionWalk,
        msg.actionInteract,
        msg.actionThrow
    );
}

void HostManager::HandleClientHeartbeat(const std::string& fromIP, uint16_t fromPort) {
    // For relay connections, fromIP is "RELAY:ROOMCODE" and fromPort is 0
    std::string clientKey;
    if (fromIP.find("RELAY:") == 0) {
        clientKey = fromIP;  // Use relay identifier as-is
    } else {
        clientKey = fromIP + ":" + std::to_string(fromPort);
    }
    
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = clients.find(clientKey);
    if (it != clients.end()) {
        it->second.lastHeartbeat = std::chrono::steady_clock::now();
    }
}

void HostManager::CleanupDisconnectedClients() {
    auto now = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(10);

    std::lock_guard<std::mutex> lock(clientsMutex);
    
    std::vector<std::string> toRemove;
    for (auto& [key, client] : clients) {
        if (!client.connected || (now - client.lastHeartbeat > timeout)) {
            toRemove.push_back(key);
        }
    }

    for (const auto& key : toRemove) {
        clients.erase(key);
        std::cout << "HostManager: Removed disconnected client: " << key << std::endl;
    }
}

void HostManager::SendInitializationPackageToAllClients() {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const auto& [key, client] : clients) {
        if (client.connected) {
            SendInitializationPackage(client.ip, client.port);
        }
    }
}

void HostManager::NotifyClientsHostReturnedToMenu() {
    HostMessageHeader msg;
    msg.type = HostMessageType::HOST_RETURNED_TO_MENU;
    memset(msg.reserved, 0, sizeof(msg.reserved));
    
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const auto& [key, client] : clients) {
        if (client.connected) {
            // Use reliable delivery for important state change messages
            SendToClientReliable(client.ip, client.port, &msg, sizeof(msg));
        }
    }
    std::cout << "HostManager: Notified all clients that host returned to menu" << std::endl;
}

void HostManager::NotifyClientsSessionEnded() {
    HostMessageHeader msg;
    msg.type = HostMessageType::HOST_SESSION_ENDED;
    memset(msg.reserved, 0, sizeof(msg.reserved));
    
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const auto& [key, client] : clients) {
        if (client.connected) {
            // Use reliable delivery for session ended message to ensure clients receive it
            SendToClientReliable(client.ip, client.port, &msg, sizeof(msg));
        }
    }
    
    // Flush ENet to ensure messages are sent before shutdown
    connectionManager.Flush();
    
    std::cout << "HostManager: Notified all clients that host session has ended" << std::endl;
}

void HostManager::SendInitializationPackage(const std::string& clientIP, uint16_t clientPort) {
    if (!engine) {
        std::cerr << "HostManager: Error - engine is null in SendInitializationPackage" << std::endl;
        return;
    }
    
    // Serialize background layers
    nlohmann::json backgroundJson = SerializeBackgroundLayers();
    
    // Serialize all objects
    nlohmann::json objectsArray = nlohmann::json::array();
    for (const auto& obj : engine->getObjects()) {
        if (obj && !obj->isMarkedForDeath()) {
            nlohmann::json objJson = SerializeObjectForSync(obj.get());
            if (!objJson.empty()) {
                objectsArray.push_back(objJson);
            }
        }
    }

    // Build initialization package
    std::string backgroundStr = backgroundJson.dump();
    std::string objectsStr = objectsArray.dump();

    // Compress the data (use best compression for initialization packages since they're large)
    std::string compressedBackground = CompressionUtils::CompressToString(backgroundStr, Z_BEST_COMPRESSION);
    std::string compressedObjects = CompressionUtils::CompressToString(objectsStr, Z_BEST_COMPRESSION);
    
    bool useCompression = !compressedBackground.empty() && !compressedObjects.empty() &&
                          (compressedBackground.size() + compressedObjects.size() < backgroundStr.size() + objectsStr.size());
    
    // Create message header
    InitPackageHeader header;
    header.header.type = HostMessageType::INIT_PACKAGE;
    memset(header.header.reserved, 0, sizeof(header.header.reserved));
    header.backgroundLayerCount = static_cast<uint32_t>(backgroundJson.is_array() ? backgroundJson.size() : (backgroundJson.empty() ? 0 : 1));
    header.objectCount = static_cast<uint32_t>(objectsArray.size());
    header.syncIntervalMs = static_cast<uint32_t>(syncInterval.count());
    header.isCompressed = useCompression ? 1 : 0;
    memset(header.reserved, 0, sizeof(header.reserved));
    
    // Build buffer
    std::vector<char> buffer;
    buffer.resize(sizeof(InitPackageHeader));
    memcpy(buffer.data(), &header, sizeof(InitPackageHeader));
    
    if (useCompression) {
        // Compressed format: size (uint32_t) + data for each block
        uint32_t bgSize = static_cast<uint32_t>(compressedBackground.size());
        uint32_t objSize = static_cast<uint32_t>(compressedObjects.size());
        
        size_t oldSize = buffer.size();
        buffer.resize(oldSize + sizeof(uint32_t) + compressedBackground.size() + sizeof(uint32_t) + compressedObjects.size());
        
        char* data = buffer.data() + oldSize;
        memcpy(data, &bgSize, sizeof(uint32_t));
        data += sizeof(uint32_t);
        memcpy(data, compressedBackground.c_str(), compressedBackground.size());
        data += compressedBackground.size();
        memcpy(data, &objSize, sizeof(uint32_t));
        data += sizeof(uint32_t);
        memcpy(data, compressedObjects.c_str(), compressedObjects.size());
    } else {
        // Uncompressed format: null-terminated strings
        size_t oldSize = buffer.size();
        buffer.resize(oldSize + backgroundStr.size() + objectsStr.size() + 2);
        
        char* data = buffer.data() + oldSize;
        memcpy(data, backgroundStr.c_str(), backgroundStr.size());
        data[backgroundStr.size()] = '\0';
        data += backgroundStr.size() + 1;
        memcpy(data, objectsStr.c_str(), objectsStr.size());
        data[objectsStr.size()] = '\0';
    }

    SendToClient(clientIP, clientPort, buffer.data(), buffer.size());
}

void HostManager::SendObjectUpdates() {
    if (!engine) {
        return;
    }

    // Send updates for all objects
    for (const auto& obj : engine->getObjects()) {
        if (!obj || obj->isMarkedForDeath()) {
            continue;
        }

        // Check if object has any syncable components
        bool hasBody = obj->hasComponent<BodyComponent>();
        bool hasSprite = obj->hasComponent<SpriteComponent>();
        bool hasSound = obj->hasComponent<SoundComponent>();
        bool hasViewGrab = obj->hasComponent<ViewGrabComponent>();

        if (!hasBody && !hasSprite && !hasSound && !hasViewGrab) {
            continue;
        }

        // Get or assign object ID
        uint32_t objectId = GetOrAssignObjectId(obj.get());

        // Serialize current state for comparison
        std::string currentState;
        if (hasBody) {
            nlohmann::json bodyJson = SerializeObjectBody(obj.get());
            currentState += bodyJson.dump() + "\n";
        }
        if (hasSprite) {
            nlohmann::json spriteJson = SerializeObjectSprite(obj.get());
            currentState += spriteJson.dump() + "\n";
        }
        if (hasSound) {
            nlohmann::json soundJson = SerializeObjectSound(obj.get());
            currentState += soundJson.dump() + "\n";
        }
        if (hasViewGrab) {
            nlohmann::json viewGrabJson = SerializeObjectViewGrab(obj.get());
            currentState += viewGrabJson.dump() + "\n";
        }

        // Check if state has changed
        {
            std::lock_guard<std::mutex> lock(stateTrackingMutex);
            auto it = lastSentState.find(objectId);
            if (it != lastSentState.end() && it->second == currentState) {
                // State hasn't changed, skip sending update
                continue;
            }
            // Update stored state (will be confirmed after successful send)
            lastSentState[objectId] = currentState;
        }

        // State has changed or is new, send update
        // Build update message
        std::vector<char> buffer(sizeof(ObjectUpdateHeader));
        ObjectUpdateHeader* header = reinterpret_cast<ObjectUpdateHeader*>(buffer.data());
        header->header.type = HostMessageType::OBJECT_UPDATE;
        memset(header->header.reserved, 0, sizeof(header->header.reserved));
        header->objectId = objectId;
        header->hasBody = hasBody ? 1 : 0;
        header->hasSprite = hasSprite ? 1 : 0;
        header->hasSound = hasSound ? 1 : 0;
        header->hasViewGrab = hasViewGrab ? 1 : 0;
        header->reserved = 0;

        // Use the already-serialized currentState
        std::string combinedData = currentState;

        // Try compression (use default compression for frequent updates to balance CPU/bandwidth)
        // Only compress if data is large enough to benefit (small packets have ENet overhead anyway)
        bool useCompression = false;
        std::string compressed;
        if (combinedData.size() > 100) {  // Only compress if data is > 100 bytes
            compressed = CompressionUtils::CompressToString(combinedData, Z_DEFAULT_COMPRESSION);
            useCompression = !compressed.empty() && compressed.size() < combinedData.size();
        }
        
        header->isCompressed = useCompression ? 1 : 0;
        
        if (useCompression) {
            // Compressed format: size (uint32_t) + compressed data
            uint32_t compressedSize = static_cast<uint32_t>(compressed.size());
            size_t oldSize = buffer.size();
            buffer.resize(oldSize + sizeof(uint32_t) + compressed.size());
            memcpy(buffer.data() + oldSize, &compressedSize, sizeof(uint32_t));
            memcpy(buffer.data() + oldSize + sizeof(uint32_t), compressed.c_str(), compressed.size());
        } else {
            // Uncompressed format: null-terminated strings (original format)
            if (hasBody) {
                nlohmann::json bodyJson = SerializeObjectBody(obj.get());
                std::string bodyStr = bodyJson.dump();
                size_t oldSize = buffer.size();
                buffer.resize(oldSize + bodyStr.size() + 1);
                memcpy(buffer.data() + oldSize, bodyStr.c_str(), bodyStr.size());
                buffer[oldSize + bodyStr.size()] = '\0';
            }
            if (hasSprite) {
                nlohmann::json spriteJson = SerializeObjectSprite(obj.get());
                std::string spriteStr = spriteJson.dump();
                size_t oldSize = buffer.size();
                buffer.resize(oldSize + spriteStr.size() + 1);
                memcpy(buffer.data() + oldSize, spriteStr.c_str(), spriteStr.size());
                buffer[oldSize + spriteStr.size()] = '\0';
            }
            if (hasSound) {
                nlohmann::json soundJson = SerializeObjectSound(obj.get());
                std::string soundStr = soundJson.dump();
                size_t oldSize = buffer.size();
                buffer.resize(oldSize + soundStr.size() + 1);
                memcpy(buffer.data() + oldSize, soundStr.c_str(), soundStr.size());
                buffer[oldSize + soundStr.size()] = '\0';
            }
            if (hasViewGrab) {
                nlohmann::json viewGrabJson = SerializeObjectViewGrab(obj.get());
                std::string viewGrabStr = viewGrabJson.dump();
                size_t oldSize = buffer.size();
                buffer.resize(oldSize + viewGrabStr.size() + 1);
                memcpy(buffer.data() + oldSize, viewGrabStr.c_str(), viewGrabStr.size());
                buffer[oldSize + viewGrabStr.size()] = '\0';
            }
        }

        // Broadcast to all clients
        BroadcastToAllClients(buffer.data(), buffer.size());
    }
}

void HostManager::SendObjectCreate(Object* obj) {
    if (!obj || obj->isMarkedForDeath()) {
        return;
    }

    uint32_t objectId = GetOrAssignObjectId(obj);
    
    // Clear any existing state tracking for this object (it's being recreated)
    {
        std::lock_guard<std::mutex> lock(stateTrackingMutex);
        lastSentState.erase(objectId);
    }
    nlohmann::json objJson = SerializeObjectForSync(obj);

    std::string objStr = objJson.dump();
    std::vector<char> buffer(sizeof(ObjectCreateHeader) + objStr.size() + 1);
    
    ObjectCreateHeader* header = reinterpret_cast<ObjectCreateHeader*>(buffer.data());
    header->header.type = HostMessageType::OBJECT_CREATE;
    memset(header->header.reserved, 0, sizeof(header->header.reserved));
    header->objectId = objectId;

    memcpy(buffer.data() + sizeof(ObjectCreateHeader), objStr.c_str(), objStr.size());
    buffer[sizeof(ObjectCreateHeader) + objStr.size()] = '\0';

    BroadcastToAllClients(buffer.data(), buffer.size());
}

void HostManager::SendObjectDestroy(Object* obj) {
    if (!obj) {
        return;
    }

    uint32_t objectId = GetOrAssignObjectId(obj);
    if (objectId == 0) {
        return;  // Object was never synced
    }

    ObjectDestroyMessage msg;
    msg.header.type = HostMessageType::OBJECT_DESTROY;
    memset(msg.header.reserved, 0, sizeof(msg.header.reserved));
    msg.objectId = objectId;
    memset(msg.reserved, 0, sizeof(msg.reserved));

    BroadcastToAllClients(&msg, sizeof(msg));

    // Remove from ID maps and state tracking
    {
        std::lock_guard<std::mutex> lock(objectIdsMutex);
        objectToId.erase(obj);
        idToObject.erase(objectId);
    }
    
    // Remove from state tracking
    {
        std::lock_guard<std::mutex> lock(stateTrackingMutex);
        lastSentState.erase(objectId);
    }
}

uint32_t HostManager::GetOrAssignObjectId(Object* obj) {
    if (!obj) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(objectIdsMutex);
    
    auto it = objectToId.find(obj);
    if (it != objectToId.end()) {
        return it->second;
    }

    uint32_t id = nextObjectId++;
    objectToId[obj] = id;
    idToObject[id] = obj;
    return id;
}

Object* HostManager::GetObjectById(uint32_t objectId) {
    std::lock_guard<std::mutex> lock(objectIdsMutex);
    auto it = idToObject.find(objectId);
    if (it != idToObject.end()) {
        return it->second;
    }
    return nullptr;
}

void HostManager::CleanupObjectIds() {
    std::lock_guard<std::mutex> lock(objectIdsMutex);
    
    std::vector<Object*> toRemove;
    for (auto& [obj, id] : objectToId) {
        if (!Object::isAlive(obj) || obj->isMarkedForDeath()) {
            toRemove.push_back(obj);
        }
    }

    std::vector<uint32_t> idsToRemove;
    for (Object* obj : toRemove) {
        auto it = objectToId.find(obj);
        if (it != objectToId.end()) {
            uint32_t id = it->second;
            idsToRemove.push_back(id);
            idToObject.erase(id);
            objectToId.erase(it);
        }
    }
    
    // Also clean up state tracking for removed objects
    {
        std::lock_guard<std::mutex> stateLock(stateTrackingMutex);
        for (uint32_t id : idsToRemove) {
            lastSentState.erase(id);
        }
    }
}

void HostManager::SendToClient(const std::string& clientIP, uint16_t clientPort, const void* data, size_t length) {
    // For relay connections, clientIP is "RELAY:ROOMCODE" and clientPort is 0
    // Use the identifier as-is for relay, otherwise construct IP:PORT
    std::string peerIdentifier;
    if (clientIP.find("RELAY:") == 0) {
        peerIdentifier = clientIP;  // Use relay identifier as-is
    } else {
        peerIdentifier = clientIP + ":" + std::to_string(clientPort);
    }
    bool sent = connectionManager.SendToPeer(peerIdentifier, data, length, false);  // Unreliable for game updates
    if (!sent) {
        std::cerr << "HostManager: Failed to send data to " << peerIdentifier << std::endl;
    } else {
        // Track sent bytes
        bytesSent.fetch_add(length);
    }
}

void HostManager::SendToClientReliable(const std::string& clientIP, uint16_t clientPort, const void* data, size_t length) {
    // For relay connections, clientIP is "RELAY:ROOMCODE" and clientPort is 0
    // Use the identifier as-is for relay, otherwise construct IP:PORT
    std::string peerIdentifier;
    if (clientIP.find("RELAY:") == 0) {
        peerIdentifier = clientIP;  // Use relay identifier as-is
    } else {
        peerIdentifier = clientIP + ":" + std::to_string(clientPort);
    }
    bool sent = connectionManager.SendToPeer(peerIdentifier, data, length, true);  // Reliable for important messages
    if (!sent) {
        std::cerr << "HostManager: Failed to send reliable data to " << peerIdentifier << std::endl;
    } else {
        // Track sent bytes
        bytesSent.fetch_add(length);
    }
}

void HostManager::BroadcastToAllClients(const void* data, size_t length) {
    // Use ConnectionManager's broadcast method
    bool sent = connectionManager.BroadcastToAllPeers(data, length, false);  // Unreliable for game updates
    if (sent) {
        // Track sent bytes (ConnectionManager handles per-peer tracking)
        bytesSent.fetch_add(length);
    }
}

nlohmann::json HostManager::SerializeBackgroundLayers() const {
    if (!engine) {
        return nlohmann::json::array();
    }

    BackgroundManager* bgManager = engine->getBackgroundManager();
    if (!bgManager) {
        return nlohmann::json::array();
    }

    return bgManager->toJson();
}

nlohmann::json HostManager::SerializeObjectForSync(Object* obj) {
    if (!obj || obj->isMarkedForDeath()) {
        return nlohmann::json();
    }

    // Use the full object JSON so the client can recreate it correctly
    // This includes all component data including fixture dimensions
    nlohmann::json j = obj->toJson();
    
    // Filter out components that could cause desync on the client
    // These components should only run on the host side
    if (j.contains("components") && j["components"].is_array()) {
        nlohmann::json filteredComponents = nlohmann::json::array();
        for (const auto& componentJson : j["components"]) {
            if (!componentJson.contains("type")) {
                continue;
            }
            
            std::string componentType = componentJson["type"].get<std::string>();
            
            // Skip components that could cause desync
            if (componentType == "CollisionDamageComponent" ||
                componentType == "RailComponent" ||
                componentType == "ObjectSpawnerComponent" ||
                componentType == "InputComponent" ||
                componentType == "JointComponent" ||
                componentType == "InteractComponent" ||
                componentType == "ThrowBehaviorComponent" ||
                componentType == "GrabBehaviorComponent" ||
                componentType == "StandardMovementBehaviorComponent" ||
                componentType == "TankMovementBehaviorComponent" ||
                componentType == "DeathTriggerComponent" ||
                componentType == "ExplodeOnDeathComponent" ||
                componentType == "SensorComponent") {
                continue;
            }
            
            filteredComponents.push_back(componentJson);
        }
        j["components"] = filteredComponents;
    }
    
    // Add object ID for tracking (not part of normal serialization)
    // This will be used by the client to map updates to objects
    uint32_t objectId = GetOrAssignObjectId(obj);
    if (objectId != 0) {
        j["_objectId"] = objectId;
    }
    
    return j;
}

nlohmann::json HostManager::SerializeObjectBody(Object* obj) const {
    nlohmann::json j;
    j["type"] = "BodyComponent";

    BodyComponent* body = obj->getComponent<BodyComponent>();
    if (!body) {
        return j;
    }

    auto [posX, posY, angleDeg] = body->getPosition();
    auto [velX, velY, angVelDeg] = body->getVelocity();

    j["posX"] = posX;
    j["posY"] = posY;
    j["angle"] = angleDeg;
    j["velX"] = velX;
    j["velY"] = velY;
    j["velAngle"] = angVelDeg;

    return j;
}

nlohmann::json HostManager::SerializeObjectSprite(Object* obj) const {
    SpriteComponent* sprite = obj->getComponent<SpriteComponent>();
    if (!sprite) {
        return nlohmann::json();
    }
    return sprite->toJson();
}

nlohmann::json HostManager::SerializeObjectSound(Object* obj) const {
    SoundComponent* sound = obj->getComponent<SoundComponent>();
    if (!sound) {
        return nlohmann::json();
    }
    return sound->toJson();
}

nlohmann::json HostManager::SerializeObjectViewGrab(Object* obj) const {
    ViewGrabComponent* viewGrab = obj->getComponent<ViewGrabComponent>();
    if (!viewGrab) {
        return nlohmann::json();
    }
    return viewGrab->toJson();
}

int HostManager::AssignPlayerToClient(const std::string& clientKey) {
    PlayerManager& playerManager = PlayerManager::getInstance();
    
    // Get set of already assigned player IDs (lock clientsMutex)
    std::unordered_set<int> assignedPlayerIds;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (const auto& [key, client] : clients) {
            if (client.connected && client.assignedPlayerId > 0) {
                assignedPlayerIds.insert(client.assignedPlayerId);
            }
        }
    }
    
    // Find the first vacant player slot (player ID without local input device)
    // Start from player 2 (player 1 is reserved for host with keyboard)
    for (int playerId = 2; playerId <= 8; ++playerId) {
        // Skip if already assigned to another client
        if (assignedPlayerIds.find(playerId) != assignedPlayerIds.end()) {
            continue;
        }
        
        // Check if this player has a local input device assigned
        std::vector<int> inputDevices = playerManager.getPlayerInputDevices(playerId);
        if (!inputDevices.empty()) {
            // This player slot has a local input device, skip it
            continue;
        }
        
        // Check if this player has a network ID assigned (already taken by another client)
        std::string networkId = playerManager.getPlayerNetworkId(playerId);
        if (!networkId.empty()) {
            // This player slot is already assigned to a network client, skip it
            continue;
        }
        
        // Found a vacant player slot! Assign it to this client
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            auto it = clients.find(clientKey);
            if (it != clients.end()) {
                it->second.assignedPlayerId = playerId;
                
                // Assign network ID to player ID in PlayerManager
                std::string networkId = clientKey; // Use "IP:PORT" as network ID
                playerManager.assignNetworkId(playerId, networkId);
                
                std::cout << "HostManager: Assigned player " << playerId << " to client " << clientKey << std::endl;
                return playerId;
            }
        }
    }
    
    std::cout << "HostManager: Warning - No available player slots for client " << clientKey << std::endl;
    return 0;
}

void HostManager::SendPlayerAssignment(const std::string& clientIP, uint16_t clientPort, int playerId) {
    AssignPlayerMessage msg;
    msg.header.type = HostMessageType::ASSIGN_PLAYER;
    memset(msg.header.reserved, 0, sizeof(msg.header.reserved));
    msg.playerId = playerId;
    memset(msg.reserved, 0, sizeof(msg.reserved));

    SendToClient(clientIP, clientPort, &msg, sizeof(msg));
    std::cout << "HostManager: Sent player assignment (Player ID: " << playerId 
             << ") to " << clientIP << ":" << clientPort << std::endl;
}

