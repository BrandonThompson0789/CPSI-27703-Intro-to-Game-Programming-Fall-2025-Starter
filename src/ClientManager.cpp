#include "ClientManager.h"
#include "Engine.h"
#include "server_manager/ServerManager.h"
#include "components/BodyComponent.h"
#include "components/SpriteComponent.h"
#include "components/SoundComponent.h"
#include "components/ViewGrabComponent.h"
#include "components/InputComponent.h"
#include "BackgroundManager.h"
#include "CollisionManager.h"
#include "CompressionUtils.h"
#include "PlayerManager.h"
#include "menus/MenuManager.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

namespace {
constexpr const char* kServerDataPath = "assets/serverData.json";

float NormalizeAngleDelta(float deltaDegrees) {
    deltaDegrees = std::fmod(deltaDegrees + 180.0f, 360.0f);
    if (deltaDegrees < 0.0f) {
        deltaDegrees += 360.0f;
    }
    return deltaDegrees - 180.0f;
}

float AdjustTargetAngle(float currentDegrees, float targetDegrees) {
    return currentDegrees + NormalizeAngleDelta(targetDegrees - currentDegrees);
}
}

ClientManager::ClientManager(Engine* engine)
    : engine(engine)
    , serverManagerSocket(INVALID_SOCKET_HANDLE)
    , serverManagerIP("127.0.0.1")
    , serverManagerPort(8888)
    , hostIP("")
    , hostPort(0)
    , hostLocalIP("")
    , hostLocalPort(0)
    , hostPublicIP("")
    , hostPublicPort(0)
    , roomCode("")
    , isConnected(false)
    , hasReceivedInitPackage(false)
    , hasVerifiedInputAfterInit(false)
    , assignedPlayerId(0)
    , smoothingEnabled(true)
    , hostSyncIntervalSeconds(0.02f)
    , smoothingRevisionCounter(0)
    , heartbeatInterval(5)  // 5 seconds
    , inputSendInterval(20)  // 20ms
    , bytesSent(0)
    , bytesReceived(0)
{
    lastHeartbeat = std::chrono::steady_clock::now();
    lastInputSend = std::chrono::steady_clock::now();
    lastBandwidthLogTime = std::chrono::steady_clock::now();
    
    // Load server data configuration
    LoadServerDataConfig();
}

ClientManager::~ClientManager() {
    Disconnect();
}

bool ClientManager::Connect(const std::string& roomCodeParam, 
                            const std::string& serverManagerIPParam, 
                            uint16_t serverManagerPortParam) {
    roomCode = roomCodeParam;
    std::transform(roomCode.begin(), roomCode.end(), roomCode.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    
    // Use provided parameters, or fall back to loaded config if defaults are used
    if (serverDataConfig.loaded) {
        // Use config values if default parameters are provided
        if (serverManagerIPParam == "127.0.0.1") {
            serverManagerIP = serverDataConfig.serverManagerIP;
        } else {
            serverManagerIP = serverManagerIPParam;
        }
        
        if (serverManagerPortParam == 8888) {
            serverManagerPort = serverDataConfig.serverManagerPort;
        } else {
            serverManagerPort = serverManagerPortParam;
        }
    } else {
        // No config loaded, use provided parameters
        serverManagerIP = serverManagerIPParam;
        serverManagerPort = serverManagerPortParam;
    }

    // Initialize ConnectionManager (ENet) for game networking
    if (!connectionManager.Initialize()) {
        std::cerr << "ClientManager: Failed to initialize ConnectionManager" << std::endl;
        return false;
    }

    // Initialize NetworkUtils for ServerManager communication
    if (!NetworkUtils::Initialize()) {
        std::cerr << "ClientManager: Failed to initialize NetworkUtils for ServerManager" << std::endl;
        connectionManager.Cleanup();
        return false;
    }

    // Create socket for ServerManager communication
    serverManagerSocket = NetworkUtils::CreateUDPSocket();
    if (serverManagerSocket == INVALID_SOCKET_HANDLE) {
        std::cerr << "ClientManager: Failed to create ServerManager socket" << std::endl;
        NetworkUtils::Cleanup();
        connectionManager.Cleanup();
        return false;
    }

    // Look up room from Server Manager
    if (!LookupRoom(roomCode, serverManagerIP, serverManagerPort)) {
        std::cerr << "ClientManager: Failed to lookup room: " << roomCode << std::endl;
        NetworkUtils::CloseSocket(serverManagerSocket);
        NetworkUtils::Cleanup();
        connectionManager.Cleanup();
        return false;
    }

    // Note: Connection will be attempted after room lookup when we have forced connection type info

    // Get the actual connected peer identifier (may differ from lookup if local connection succeeded)
    std::string hostPeerIdentifier = connectionManager.GetFirstConnectedPeerIdentifier();
    if (hostPeerIdentifier.empty()) {
        std::cerr << "ClientManager: No peer identifier after connection" << std::endl;
        connectionManager.DisconnectFromHost();
        NetworkUtils::CloseSocket(serverManagerSocket);
        NetworkUtils::Cleanup();
        connectionManager.Cleanup();
        return false;
    }

    // Parse IP and port from peer identifier to update hostIP/hostPort
    // Check if it's a relay connection first
    if (hostPeerIdentifier.find("RELAY:") == 0) {
        // Relay mode - use room code as identifier, port is 0
        hostIP = hostPeerIdentifier;
        hostPort = 0;
    } else {
        size_t colonPos = hostPeerIdentifier.find(':');
        if (colonPos != std::string::npos) {
            hostIP = hostPeerIdentifier.substr(0, colonPos);
            hostPort = static_cast<uint16_t>(std::stoi(hostPeerIdentifier.substr(colonPos + 1)));
        } else {
            // Fallback
            hostIP = hostPeerIdentifier;
            hostPort = 0;
        }
    }

    std::cout << "ClientManager: Using connected peer identifier: " << hostPeerIdentifier << std::endl;

    // Send CLIENT_CONNECT message to host
    ClientConnectMessage msg;
    msg.header.type = HostMessageType::CLIENT_CONNECT;
    memset(msg.header.reserved, 0, sizeof(msg.header.reserved));
    memset(msg.reserved, 0, sizeof(msg.reserved));

    // Wait for player assignment (with timeout)
    auto startTime = std::chrono::steady_clock::now();
    const auto totalTimeout = std::chrono::seconds(10);

    while (std::chrono::steady_clock::now() - startTime < totalTimeout) {
        // Send connection message periodically
        // Use SendToFirstPeer since client only has one peer (the host)
        connectionManager.SendToFirstPeer(&msg, sizeof(msg), true);
        
        connectionManager.Update(0.0f);  // Process events
        ProcessIncomingMessages();
        
        // Connected when we receive player assignment
        if (isConnected) {
            std::cout << "ClientManager: Connected to host" << std::endl;
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cerr << "ClientManager: Timeout waiting for connection" << std::endl;
    connectionManager.DisconnectFromHost();
    NetworkUtils::CloseSocket(serverManagerSocket);
    NetworkUtils::Cleanup();
    connectionManager.Cleanup();
    return false;
}

void ClientManager::Update(float deltaTime) {
    if (!isConnected) {
        return;
    }

    // Update ConnectionManager (processes ENet events)
    connectionManager.Update(deltaTime);

    // Process incoming messages
    ProcessIncomingMessages();

        // After init package is received, verify input assignment on objects that are now in the engine
        // (objects are queued when created, then added to engine in next frame)
        if (hasReceivedInitPackage && assignedPlayerId > 0 && engine && !hasVerifiedInputAfterInit) {
            // Check both queued objects and objects already in the engine
            // (when level is already loaded, objects might be queued or already added)
            bool foundInputComponent = false;
            
            // Check queued objects first (newly created from init package)
            for (auto& obj : engine->getQueuedObjects()) {
                if (obj && obj->hasComponent<InputComponent>()) {
                    InputComponent* inputComp = obj->getComponent<InputComponent>();
                    if (inputComp && inputComp->getPlayerId() == assignedPlayerId) {
                        foundInputComponent = true;
                        break;
                    }
                }
            }
            
            // Also check objects already in the engine
            if (!foundInputComponent) {
                for (auto& obj : engine->getObjects()) {
                    if (obj && obj->hasComponent<InputComponent>()) {
                        InputComponent* inputComp = obj->getComponent<InputComponent>();
                        if (inputComp && inputComp->getPlayerId() == assignedPlayerId) {
                            foundInputComponent = true;
                            break;
                        }
                    }
                }
            }
            
            // If we found an InputComponent with our player ID, verify input is assigned
            if (foundInputComponent) {
                PlayerManager::getInstance().assignInputDevice(assignedPlayerId, INPUT_SOURCE_KEYBOARD);
                std::cout << "ClientManager: Verified input assignment after objects added to engine (player ID " << assignedPlayerId << ")" << std::endl;
                hasVerifiedInputAfterInit = true;
            }
        }

    // Log bandwidth statistics once per second
    auto now = std::chrono::steady_clock::now();
    if (now - lastBandwidthLogTime >= std::chrono::seconds(1)) {
        uint64_t sent = bytesSent.exchange(0);
        uint64_t received = bytesReceived.exchange(0);
        double sentKB = static_cast<double>(sent) / 1024.0;
        double receivedKB = static_cast<double>(received) / 1024.0;
        std::cout << "ClientManager: Bandwidth (last second) - Sent: " << sentKB << " KB, Received: " << receivedKB << " KB" << std::endl;
        lastBandwidthLogTime = now;
    }

    // Send heartbeat periodically
    if (now - lastHeartbeat >= heartbeatInterval) {
        SendHeartbeat();
        lastHeartbeat = now;
    }

    // Send input periodically
    if (now - lastInputSend >= inputSendInterval) {
        SendInput();
        lastInputSend = now;
    }

    ApplySmoothing(deltaTime);

    // Cleanup object IDs for destroyed objects
    CleanupObjectIds();
}

void ClientManager::Disconnect() {
    if (!isConnected) {
        return;
    }

    // Send disconnect message
    if (!hostIP.empty() && hostPort > 0) {
        ClientConnectMessage msg;
        msg.header.type = HostMessageType::CLIENT_DISCONNECT;
        memset(msg.header.reserved, 0, sizeof(msg.header.reserved));
        memset(msg.reserved, 0, sizeof(msg.reserved));
        
        std::string hostPeerIdentifier = hostIP + ":" + std::to_string(hostPort);
        connectionManager.SendToPeer(hostPeerIdentifier, &msg, sizeof(msg), true);
    }

    isConnected = false;
    hasReceivedInitPackage = false;

    // Disconnect from host
    connectionManager.DisconnectFromHost();

    // Close ServerManager socket
    if (serverManagerSocket != INVALID_SOCKET_HANDLE) {
        NetworkUtils::CloseSocket(serverManagerSocket);
        serverManagerSocket = INVALID_SOCKET_HANDLE;
    }

    {
        std::lock_guard<std::mutex> lock(objectIdsMutex);
        idToObject.clear();
    }

    ClearAllSmoothingStates();

    NetworkUtils::Cleanup();
    connectionManager.Cleanup();
    std::cout << "ClientManager: Disconnected" << std::endl;
}

bool ClientManager::LookupRoom(const std::string& roomCodeParam, 
                               const std::string& serverManagerIPParam, 
                               uint16_t serverManagerPortParam) {
    // Create lookup message
    ClientLookupMessage msg;
    msg.header.type = MessageType::CLIENT_LOOKUP;
    memset(msg.header.reserved, 0, sizeof(msg.header.reserved));
    strncpy(msg.roomCode, roomCodeParam.c_str(), sizeof(msg.roomCode) - 1);
    msg.roomCode[sizeof(msg.roomCode) - 1] = '\0';

    // Send lookup request (use ServerManager socket)
    NetworkUtils::SendTo(serverManagerSocket, &msg, sizeof(msg), serverManagerIPParam, serverManagerPortParam);

    // Wait for response (with timeout)
    auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(5);

    char buffer[4096];
    std::string fromIP;
    uint16_t fromPort;

    while (std::chrono::steady_clock::now() - startTime < timeout) {
        int received = NetworkUtils::ReceiveFrom(serverManagerSocket, buffer, sizeof(buffer), fromIP, fromPort);
        
        if (received >= static_cast<int>(sizeof(RoomInfoResponse))) {
            const RoomInfoResponse* response = reinterpret_cast<const RoomInfoResponse*>(buffer);
            if (response->header.type == MessageType::RESPONSE_ROOM_INFO) {
                // Store both local and public IPs for fallback mechanism
                hostLocalIP = std::string(response->hostIP);
                hostLocalPort = response->hostPort;
                
                if (response->hostPublicIP[0] != '\0') {
                    hostPublicIP = std::string(response->hostPublicIP);
                    hostPublicPort = response->hostPublicPort;
                    std::cout << "ClientManager: Found room " << roomCodeParam 
                             << " - Public: " << hostPublicIP << ":" << hostPublicPort 
                             << ", Local: " << hostLocalIP << ":" << hostLocalPort << std::endl;
                } else {
                    // No public IP available, use local only
                    hostPublicIP = "";
                    hostPublicPort = 0;
                    std::cout << "ClientManager: Found room " << roomCodeParam 
                             << " at " << hostLocalIP << ":" << hostLocalPort 
                             << " (no public IP available)" << std::endl;
                }
                
                // Store forced connection type and relay enabled status
                uint8_t forcedType = response->forcedConnectionType;
                bool relayEnabled = response->relayEnabled != 0;
                
                // Connect to host using ConnectionManager with forced connection type
                if (!connectionManager.ConnectToHost(roomCode, serverManagerIP, serverManagerPort,
                                                    hostPublicIP, hostPublicPort,
                                                    hostLocalIP, hostLocalPort,
                                                    forcedType, relayEnabled)) {
                    std::cerr << "ClientManager: Failed to connect to host" << std::endl;
                    NetworkUtils::CloseSocket(serverManagerSocket);
                    NetworkUtils::Cleanup();
                    connectionManager.Cleanup();
                    return false;
                }
                
                return true;
            } else if (response->header.type == MessageType::RESPONSE_ERROR) {
                const ErrorResponse* errorResponse = reinterpret_cast<const ErrorResponse*>(buffer);
                std::cerr << "ClientManager: Server Manager error: " << errorResponse->errorMessage << std::endl;
                return false;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cerr << "ClientManager: Timeout waiting for Server Manager response" << std::endl;
    return false;
}

void ClientManager::ProcessIncomingMessages() {
    // Use larger buffer for initialization package (can be large with many objects)
    char buffer[65536];  // Max UDP packet size
    std::string fromPeerIdentifier;
    size_t received;

    while (true) {
        if (!connectionManager.Receive(buffer, sizeof(buffer), received, fromPeerIdentifier)) {
            break;
        }

        // Track received bytes
        bytesReceived.fetch_add(received);

        // Parse IP and port from peer identifier (format: "IP:PORT" or "RELAY:ROOMCODE")
        std::string fromIP;
        uint16_t fromPort = 0;
        size_t colonPos = fromPeerIdentifier.find(':');
        if (colonPos != std::string::npos) {
            std::string prefix = fromPeerIdentifier.substr(0, colonPos);
            if (prefix == "RELAY") {
                // Relay mode - accept from relay
                fromIP = fromPeerIdentifier;
            } else {
                fromIP = prefix;
                fromPort = static_cast<uint16_t>(std::stoi(fromPeerIdentifier.substr(colonPos + 1)));
            }
        } else {
            fromIP = fromPeerIdentifier;  // Fallback
        }

        // Verify message is from host
        // During connection, accept messages from either public or local IP
        // After connection, only accept from the active IP or relay
        bool isValidSource = false;
        if (isConnected) {
            // After connection, accept from active IP or relay
            isValidSource = (fromIP == hostIP && fromPort == hostPort) ||
                           (fromPeerIdentifier.find("RELAY:") == 0);
        } else {
            // During connection, accept from either IP (in case we're trying fallback)
            isValidSource = ((fromIP == hostIP && fromPort == hostPort) ||
                            (!hostPublicIP.empty() && fromIP == hostPublicIP && fromPort == hostPublicPort) ||
                            (!hostLocalIP.empty() && fromIP == hostLocalIP && fromPort == hostLocalPort) ||
                            (fromPeerIdentifier.find("RELAY:") == 0));
            
            // If we receive from a different IP than we're currently trying, switch to it
            if (isValidSource && fromPeerIdentifier.find("RELAY:") != 0 && 
                (fromIP != hostIP || fromPort != hostPort)) {
                std::cout << "ClientManager: Received response from " << fromIP << ":" << fromPort 
                         << ", switching to this address" << std::endl;
                hostIP = fromIP;
                hostPort = fromPort;
            }
        }
        
        if (!isValidSource) {
            continue;
        }

        if (received < sizeof(HostMessageHeader)) {
            continue;
        }

        const HostMessageHeader* header = reinterpret_cast<const HostMessageHeader*>(buffer);

        switch (header->type) {
            case HostMessageType::INIT_PACKAGE:
                HandleInitPackage(buffer, received);
                hasReceivedInitPackage = true;
                break;

            case HostMessageType::OBJECT_UPDATE:
                HandleObjectUpdate(buffer, received);
                break;

            case HostMessageType::OBJECT_CREATE:
                HandleObjectCreate(buffer, received);
                break;

            case HostMessageType::OBJECT_DESTROY:
                if (received >= static_cast<int>(sizeof(ObjectDestroyMessage))) {
                    HandleObjectDestroy(*reinterpret_cast<const ObjectDestroyMessage*>(buffer));
                }
                break;

            case HostMessageType::ASSIGN_PLAYER:
                if (received >= static_cast<int>(sizeof(AssignPlayerMessage))) {
                    const AssignPlayerMessage* msg = reinterpret_cast<const AssignPlayerMessage*>(buffer);
                    assignedPlayerId = msg->playerId;
                    isConnected = true;  // Connected when we receive player assignment
                    std::cout << "ClientManager: Assigned to player ID: " << assignedPlayerId << std::endl;
                    
                    // Assign keyboard to this player (client uses keyboard by default)
                    PlayerManager::getInstance().assignInputDevice(assignedPlayerId, INPUT_SOURCE_KEYBOARD);
                } else {
                    std::cerr << "ClientManager: ASSIGN_PLAYER message too small: " << received << " bytes" << std::endl;
                }
                break;

            case HostMessageType::HOST_RETURNED_TO_MENU:
                HandleHostReturnedToMenu();
                break;

            case HostMessageType::HOST_SESSION_ENDED:
                HandleHostSessionEnded();
                break;

            default:
                break;
        }
    }
}

void ClientManager::HandleInitPackage(const void* data, size_t length) {
    if (length < sizeof(InitPackageHeader)) {
        std::cerr << "ClientManager: Init package too small: " << length << " bytes" << std::endl;
        return;
    }

    const InitPackageHeader* header = reinterpret_cast<const InitPackageHeader*>(data);
    if (header->syncIntervalMs > 0) {
        hostSyncIntervalSeconds = static_cast<float>(header->syncIntervalMs) / 1000.0f;
    }
    
    std::string backgroundStr;
    std::string objectsStr;
    
    const char* payloadData = reinterpret_cast<const char*>(data) + sizeof(InitPackageHeader);
    size_t offset = 0;
    
    if (header->isCompressed) {
        // Compressed format: size (uint32_t) + data for each block
        if (length < sizeof(InitPackageHeader) + sizeof(uint32_t)) {
            std::cerr << "ClientManager: Compressed init package too small" << std::endl;
            return;
        }
        
        // Read background size and data
        uint32_t bgSize = *reinterpret_cast<const uint32_t*>(payloadData + offset);
        offset += sizeof(uint32_t);
        
        if (length < sizeof(InitPackageHeader) + offset + bgSize) {
            std::cerr << "ClientManager: Compressed init package background data truncated" << std::endl;
            return;
        }
        
        std::string compressedBackground(payloadData + offset, bgSize);
        offset += bgSize;
        
        // Read objects size and data
        if (length < sizeof(InitPackageHeader) + offset + sizeof(uint32_t)) {
            std::cerr << "ClientManager: Compressed init package objects header truncated" << std::endl;
            return;
        }
        
        uint32_t objSize = *reinterpret_cast<const uint32_t*>(payloadData + offset);
        offset += sizeof(uint32_t);
        
        if (length < sizeof(InitPackageHeader) + offset + objSize) {
            std::cerr << "ClientManager: Compressed init package objects data truncated" << std::endl;
            return;
        }
        
        std::string compressedObjects(payloadData + offset, objSize);
        
        // Decompress
        backgroundStr = CompressionUtils::DecompressFromString(compressedBackground);
        objectsStr = CompressionUtils::DecompressFromString(compressedObjects);
        
        if (backgroundStr.empty() || objectsStr.empty()) {
            std::cerr << "ClientManager: Failed to decompress init package data" << std::endl;
            return;
        }
    } else {
        // Uncompressed format: null-terminated strings
        backgroundStr = std::string(payloadData + offset);
        offset += backgroundStr.length() + 1;
        objectsStr = std::string(payloadData + offset);
    }

    try {
        // Load background layers
        if (!backgroundStr.empty()) {
            nlohmann::json backgroundJson = nlohmann::json::parse(backgroundStr);
            if (engine && engine->getBackgroundManager()) {
                engine->getBackgroundManager()->loadFromJson(backgroundJson, engine);
            }
        }

        // Clear existing objects
        if (engine) {
            engine->getObjects().clear();
            engine->getQueuedObjects().clear();
            if (engine->getCollisionManager()) {
                engine->getCollisionManager()->clearImpacts();
            }
        }

        ClearAllSmoothingStates();
        
        // Reset verification flag when new init package is received
        hasVerifiedInputAfterInit = false;

        // Set Engine instance
        if (engine) {
            Object::setEngine(engine);
        }

        // Ensure input device is assigned to our player ID (in case init package arrives before or after ASSIGN_PLAYER)
        if (assignedPlayerId > 0) {
            PlayerManager::getInstance().assignInputDevice(assignedPlayerId, INPUT_SOURCE_KEYBOARD);
            std::cout << "ClientManager: Re-assigned keyboard to player ID " << assignedPlayerId << " after level load" << std::endl;
        }

        // Load objects from JSON
        nlohmann::json objectsJson = nlohmann::json::parse(objectsStr);
        if (objectsJson.is_array()) {
            for (const auto& objJson : objectsJson) {
                if (!objJson.contains("components") || !objJson["components"].is_array()) {
                    continue;
                }

                // Extract object ID from JSON if present (from host's serialization)
                uint32_t objectId = 0;
                if (objJson.contains("_objectId")) {
                    objectId = objJson["_objectId"].get<uint32_t>();
                } else {
                    // Fallback: assign sequential IDs starting from 1
                    static uint32_t fallbackId = 1;
                    objectId = fallbackId++;
                }

                CreateObjectFromJson(objectId, objJson);
            }
        }

        // After objects are created, verify input assignment immediately
        // This ensures that objects created with the client's assigned player ID have their InputComponents working
        // This is especially important when the level is already loaded and init package arrives immediately
        if (assignedPlayerId > 0 && engine) {
            // Check queued objects (objects are queued before being added to engine)
            bool foundInputComponent = false;
            for (auto& obj : engine->getQueuedObjects()) {
                if (obj && obj->hasComponent<InputComponent>()) {
                    InputComponent* inputComp = obj->getComponent<InputComponent>();
                    if (inputComp && inputComp->getPlayerId() == assignedPlayerId) {
                        foundInputComponent = true;
                        break;
                    }
                }
            }
            
            // Also check objects already in the engine (in case they were added synchronously)
            if (!foundInputComponent) {
                for (auto& obj : engine->getObjects()) {
                    if (obj && obj->hasComponent<InputComponent>()) {
                        InputComponent* inputComp = obj->getComponent<InputComponent>();
                        if (inputComp && inputComp->getPlayerId() == assignedPlayerId) {
                            foundInputComponent = true;
                            break;
                        }
                    }
                }
            }
            
            // If we found an InputComponent with our player ID, ensure input is assigned
            if (foundInputComponent) {
                PlayerManager::getInstance().assignInputDevice(assignedPlayerId, INPUT_SOURCE_KEYBOARD);
                std::cout << "ClientManager: Verified input assignment immediately after init package (player ID " << assignedPlayerId << ")" << std::endl;
                // Mark as verified so Update() doesn't need to check again
                hasVerifiedInputAfterInit = true;
            }
        }

        isConnected = true;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "ClientManager: JSON parsing error in init package: " << e.what() << std::endl;
    }
}

void ClientManager::HandleObjectUpdate(const void* data, size_t length) {
    if (length < sizeof(ObjectUpdateHeader)) {
        return;
    }

    const ObjectUpdateHeader* header = reinterpret_cast<const ObjectUpdateHeader*>(data);
    uint32_t objectId = header->objectId;

    nlohmann::json bodyJson, spriteJson, soundJson, viewGrabJson;
    const char* payloadData = reinterpret_cast<const char*>(data) + sizeof(ObjectUpdateHeader);
    size_t offset = 0;

    if (header->isCompressed) {
        // Compressed format: size (uint32_t) + compressed data
        if (length < sizeof(ObjectUpdateHeader) + sizeof(uint32_t)) {
            return;
        }
        
        uint32_t compressedSize = *reinterpret_cast<const uint32_t*>(payloadData + offset);
        offset += sizeof(uint32_t);
        
        if (length < sizeof(ObjectUpdateHeader) + offset + compressedSize) {
            return;
        }
        
        std::string compressedData(payloadData + offset, compressedSize);
        std::string decompressed = CompressionUtils::DecompressFromString(compressedData);
        
        if (decompressed.empty()) {
            return;
        }
        
        // Parse the decompressed data (components separated by newlines)
        std::istringstream stream(decompressed);
        std::string line;
        int componentIndex = 0;
        
        while (std::getline(stream, line) && !line.empty()) {
            try {
                nlohmann::json json = nlohmann::json::parse(line);
                if (componentIndex == 0 && header->hasBody) {
                    bodyJson = json;
                } else if (componentIndex == 1 && header->hasSprite) {
                    spriteJson = json;
                } else if (componentIndex == 2 && header->hasSound) {
                    soundJson = json;
                } else if (componentIndex == 3 && header->hasViewGrab) {
                    viewGrabJson = json;
                }
                componentIndex++;
            } catch (const nlohmann::json::exception&) {
                // Skip invalid JSON
            }
        }
    } else {
        // Uncompressed format: null-terminated strings
        if (header->hasBody) {
            std::string bodyStr(payloadData + offset);
            offset += bodyStr.length() + 1;
            try {
                bodyJson = nlohmann::json::parse(bodyStr);
            } catch (const nlohmann::json::exception&) {
                // Skip invalid JSON
            }
        }

        if (header->hasSprite) {
            std::string spriteStr(payloadData + offset);
            offset += spriteStr.length() + 1;
            try {
                spriteJson = nlohmann::json::parse(spriteStr);
            } catch (const nlohmann::json::exception&) {
                // Skip invalid JSON
            }
        }

        if (header->hasSound) {
            std::string soundStr(payloadData + offset);
            offset += soundStr.length() + 1;
            try {
                soundJson = nlohmann::json::parse(soundStr);
            } catch (const nlohmann::json::exception&) {
                // Skip invalid JSON
            }
        }

        if (header->hasViewGrab) {
            std::string viewGrabStr(payloadData + offset);
            offset += viewGrabStr.length() + 1;
            try {
                viewGrabJson = nlohmann::json::parse(viewGrabStr);
            } catch (const nlohmann::json::exception&) {
                // Skip invalid JSON
            }
        }
    }

    UpdateObjectFromJson(objectId, bodyJson, spriteJson, soundJson, viewGrabJson);
}

void ClientManager::HandleObjectCreate(const void* data, size_t length) {
    if (length < sizeof(ObjectCreateHeader)) {
        return;
    }

    const ObjectCreateHeader* header = reinterpret_cast<const ObjectCreateHeader*>(data);
    uint32_t objectId = header->objectId;

    // Parse JSON object definition
    const char* jsonData = reinterpret_cast<const char*>(data) + sizeof(ObjectCreateHeader);
    std::string objStr(jsonData);

    try {
        nlohmann::json objJson = nlohmann::json::parse(objStr);
        CreateObjectFromJson(objectId, objJson);
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "ClientManager: JSON parsing error in object create: " << e.what() << std::endl;
    }
}

void ClientManager::HandleObjectDestroy(const ObjectDestroyMessage& msg) {
    DestroyObject(msg.objectId);
}

void ClientManager::HandleHostReturnedToMenu() {
    std::cout << "ClientManager: Host returned to menu - clearing level and showing waiting screen" << std::endl;
    
    // Clear existing objects and background
    if (engine) {
        engine->getObjects().clear();
        engine->getQueuedObjects().clear();
        if (engine->getCollisionManager()) {
            engine->getCollisionManager()->clearImpacts();
        }
        if (engine->getBackgroundManager()) {
            // Clear background by loading empty background
            nlohmann::json emptyBackground = nlohmann::json::object();
            engine->getBackgroundManager()->loadFromJson(emptyBackground, engine);
        }
    }
    
    // Clear object ID mappings
    {
        std::lock_guard<std::mutex> lock(objectIdsMutex);
        idToObject.clear();
    }
    
    // Clear smoothing states
    ClearAllSmoothingStates();
    
    // Reset init package flag so waiting screen knows to wait
    hasReceivedInitPackage = false;
    hasVerifiedInputAfterInit = false;
    
    // Show waiting screen
    if (engine && engine->getMenuManager()) {
        engine->getMenuManager()->openMenu("waiting_for_host");
    }
}

void ClientManager::HandleHostSessionEnded() {
    std::cout << "ClientManager: Host session ended - showing session ended menu" << std::endl;
    
    // Mark as disconnected
    isConnected = false;
    hasReceivedInitPackage = false;
    hasVerifiedInputAfterInit = false;
    
    // Clear object ID mappings
    {
        std::lock_guard<std::mutex> lock(objectIdsMutex);
        idToObject.clear();
    }
    
    // Clear smoothing states
    ClearAllSmoothingStates();
    
    // Show session ended menu
    if (engine && engine->getMenuManager()) {
        engine->getMenuManager()->openMenu("host_session_ended");
    }
}

void ClientManager::CreateObjectFromJson(uint32_t objectId, const nlohmann::json& objJson) {
    if (!engine) {
        return;
    }

    auto object = std::make_unique<Object>();
    
    // Set name if present
    if (objJson.contains("name")) {
        object->setName(objJson["name"].get<std::string>());
    }

    // Create object from JSON
    object->fromJson(objJson);

    // Store object ID mapping
    {
        std::lock_guard<std::mutex> lock(objectIdsMutex);
        idToObject[objectId] = object.get();
    }

    // Add to engine
    engine->queueObject(std::move(object));
}

void ClientManager::UpdateObjectFromJson(uint32_t objectId, 
                                        const nlohmann::json& bodyJson, 
                                        const nlohmann::json& spriteJson, 
                                        const nlohmann::json& soundJson,
                                        const nlohmann::json& viewGrabJson) {
    Object* obj = GetObjectById(objectId);
    if (!obj) {
        return;
    }

    // Update BodyComponent
    if (!bodyJson.empty() && obj->hasComponent<BodyComponent>()) {
        BodyComponent* body = obj->getComponent<BodyComponent>();
        bool smoothed = UpdateSmoothingState(objectId, body, bodyJson);

        if (!smoothed) {
            if (bodyJson.contains("posX") && bodyJson.contains("posY") && bodyJson.contains("angle")) {
                float posX = bodyJson["posX"].get<float>();
                float posY = bodyJson["posY"].get<float>();
                float angle = bodyJson["angle"].get<float>();
                body->setPosition(posX, posY, angle);
            }
            if (bodyJson.contains("velX") && bodyJson.contains("velY") && bodyJson.contains("velAngle")) {
                float velX = bodyJson["velX"].get<float>();
                float velY = bodyJson["velY"].get<float>();
                float velAngle = bodyJson["velAngle"].get<float>();
                body->setVelocity(velX, velY, velAngle);
            }
        }
    }

    // Update SpriteComponent (components don't have fromJson, so we update properties directly)
    // For now, sprite updates are handled through the body position updates
    // More complex sprite state updates would require component-specific update methods
    
    // SoundComponent and ViewGrabComponent updates are typically not needed during gameplay
    // as they are mostly event-driven or camera-related
}

void ClientManager::DestroyObject(uint32_t objectId) {
    Object* obj = GetObjectById(objectId);
    if (obj) {
        obj->markForDeath();
    }

    ClearSmoothingState(objectId);

    {
        std::lock_guard<std::mutex> lock(objectIdsMutex);
        idToObject.erase(objectId);
    }
}

void ClientManager::SendInput() {
    if (assignedPlayerId <= 0) {
        return;
    }

    // Get input from PlayerManager for the assigned player
    PlayerManager& playerManager = PlayerManager::getInstance();
    
    // Ensure input device is assigned (in case it wasn't set up correctly)
    if (!playerManager.isPlayerAssigned(assignedPlayerId)) {
        playerManager.assignInputDevice(assignedPlayerId, INPUT_SOURCE_KEYBOARD);
    }
    
    // Get input values from PlayerManager
    float moveUp = playerManager.getInputValue(assignedPlayerId, GameAction::MOVE_UP);
    float moveDown = playerManager.getInputValue(assignedPlayerId, GameAction::MOVE_DOWN);
    float moveLeft = playerManager.getInputValue(assignedPlayerId, GameAction::MOVE_LEFT);
    float moveRight = playerManager.getInputValue(assignedPlayerId, GameAction::MOVE_RIGHT);
    float actionWalk = playerManager.getInputValue(assignedPlayerId, GameAction::ACTION_WALK);
    float actionInteract = playerManager.getInputValue(assignedPlayerId, GameAction::ACTION_INTERACT);
    float actionThrow = playerManager.getInputValue(assignedPlayerId, GameAction::ACTION_THROW);

    // Send input message
    ClientInputMessage msg;
    msg.header.type = HostMessageType::CLIENT_INPUT;
    memset(msg.header.reserved, 0, sizeof(msg.header.reserved));
    msg.playerId = assignedPlayerId;
    msg.moveUp = moveUp;
    msg.moveDown = moveDown;
    msg.moveLeft = moveLeft;
    msg.moveRight = moveRight;
    msg.actionWalk = actionWalk;
    msg.actionInteract = actionInteract;
    msg.actionThrow = actionThrow;

    SendToHost(&msg, sizeof(msg));
}

void ClientManager::SendHeartbeat() {
    ClientConnectMessage msg;
    msg.header.type = HostMessageType::HEARTBEAT;
    memset(msg.header.reserved, 0, sizeof(msg.header.reserved));
    memset(msg.reserved, 0, sizeof(msg.reserved));

    SendToHost(&msg, sizeof(msg));
}

void ClientManager::SendToHost(const void* data, size_t length) {
    // Use SendToFirstPeer since client only has one peer (the host)
    // This avoids identifier mismatch issues between client and host
    bool sent = connectionManager.SendToFirstPeer(data, length, false);  // Unreliable for game updates
    if (!sent && connectionManager.IsUsingRelay()) {
        // If direct failed and we're using relay, try relay identifier
        std::string relayIdentifier = "RELAY:" + roomCode;
        sent = connectionManager.SendToPeer(relayIdentifier, data, length, false);
    }
    if (sent) {
        // Track sent bytes
        bytesSent.fetch_add(length);
    }
}

Object* ClientManager::GetObjectById(uint32_t objectId) {
    std::lock_guard<std::mutex> lock(objectIdsMutex);
    auto it = idToObject.find(objectId);
    if (it != idToObject.end()) {
        return it->second;
    }
    return nullptr;
}

void ClientManager::CleanupObjectIds() {
    std::lock_guard<std::mutex> lock(objectIdsMutex);
    
    std::vector<uint32_t> toRemove;
    for (auto& [id, obj] : idToObject) {
        if (!Object::isAlive(obj) || obj->isMarkedForDeath()) {
            toRemove.push_back(id);
        }
    }

    for (uint32_t id : toRemove) {
        idToObject.erase(id);
    }
}

void ClientManager::SetSmoothingEnabled(bool enabled) {
    if (smoothingEnabled == enabled) {
        return;
    }

    smoothingEnabled = enabled;
    if (!smoothingEnabled) {
        ClearAllSmoothingStates();
    }
}

bool ClientManager::UpdateSmoothingState(uint32_t objectId, BodyComponent* body, const nlohmann::json& bodyJson) {
    if (!smoothingEnabled || !body || hostSyncIntervalSeconds <= 0.0f) {
        ClearSmoothingState(objectId);
        return false;
    }

    bool hasPosition = bodyJson.contains("posX") && bodyJson.contains("posY") && bodyJson.contains("angle");
    bool hasVelocity = bodyJson.contains("velX") && bodyJson.contains("velY") && bodyJson.contains("velAngle");
    if (!hasPosition && !hasVelocity) {
        return false;
    }

    auto [currPosX, currPosY, currAngle] = body->getPosition();
    auto [currVelX, currVelY, currVelAngle] = body->getVelocity();

    ObjectSmoothingState state;
    state.startPosX = currPosX;
    state.startPosY = currPosY;
    state.startAngle = currAngle;
    state.startVelX = currVelX;
    state.startVelY = currVelY;
    state.startVelAngle = currVelAngle;

    if (hasPosition) {
        state.targetPosX = bodyJson["posX"].get<float>();
        state.targetPosY = bodyJson["posY"].get<float>();
        float rawTargetAngle = bodyJson["angle"].get<float>();
        state.targetAngle = AdjustTargetAngle(currAngle, rawTargetAngle);
    } else {
        state.targetPosX = currPosX;
        state.targetPosY = currPosY;
        state.targetAngle = currAngle;
    }

    state.targetVelX = hasVelocity ? bodyJson["velX"].get<float>() : currVelX;
    state.targetVelY = hasVelocity ? bodyJson["velY"].get<float>() : currVelY;
    state.targetVelAngle = hasVelocity ? bodyJson["velAngle"].get<float>() : currVelAngle;

    state.elapsed = 0.0f;
    state.duration = std::max(hostSyncIntervalSeconds, 0.001f);
    state.revision = ++smoothingRevisionCounter;

    {
        std::lock_guard<std::mutex> lock(smoothingMutex);
        smoothingStates[objectId] = state;
    }

    return true;
}

void ClientManager::ApplySmoothing(float deltaTime) {
    if (!smoothingEnabled) {
        return;
    }

    struct SmoothingStateCopy {
        uint32_t objectId;
        ObjectSmoothingState state;
    };

    std::vector<SmoothingStateCopy> copies;
    {
        std::lock_guard<std::mutex> lock(smoothingMutex);
        if (smoothingStates.empty()) {
            return;
        }

        copies.reserve(smoothingStates.size());
        for (const auto& entry : smoothingStates) {
            copies.push_back({entry.first, entry.second});
        }
    }

    if (copies.empty()) {
        return;
    }

    std::vector<SmoothingStateCopy> inProgress;
    std::vector<SmoothingStateCopy> finished;
    inProgress.reserve(copies.size());

    auto lerp = [](float a, float b, float alpha) {
        return a + (b - a) * alpha;
    };

    for (auto& entry : copies) {
        Object* obj = GetObjectById(entry.objectId);
        if (!obj || !obj->hasComponent<BodyComponent>()) {
            finished.push_back(entry);
            continue;
        }

        BodyComponent* body = obj->getComponent<BodyComponent>();
        if (!body) {
            finished.push_back(entry);
            continue;
        }

        float duration = entry.state.duration > 0.0f ? entry.state.duration : hostSyncIntervalSeconds;
        if (duration <= 0.0f) {
            duration = 0.02f;
        }

        entry.state.elapsed = std::min(entry.state.elapsed + deltaTime, duration);
        float t = duration > 0.0f ? entry.state.elapsed / duration : 1.0f;

        float posX = lerp(entry.state.startPosX, entry.state.targetPosX, t);
        float posY = lerp(entry.state.startPosY, entry.state.targetPosY, t);
        float angle = lerp(entry.state.startAngle, entry.state.targetAngle, t);
        float velX = lerp(entry.state.startVelX, entry.state.targetVelX, t);
        float velY = lerp(entry.state.startVelY, entry.state.targetVelY, t);
        float velAngle = lerp(entry.state.startVelAngle, entry.state.targetVelAngle, t);

        body->setPosition(posX, posY, angle);
        body->setVelocity(velX, velY, velAngle);

        if (t >= 0.999f) {
            finished.push_back(entry);
        } else {
            inProgress.push_back(entry);
        }
    }

    {
        std::lock_guard<std::mutex> lock(smoothingMutex);
        for (const auto& entry : finished) {
            auto it = smoothingStates.find(entry.objectId);
            if (it != smoothingStates.end() && it->second.revision == entry.state.revision) {
                smoothingStates.erase(it);
            }
        }

        for (const auto& entry : inProgress) {
            auto it = smoothingStates.find(entry.objectId);
            if (it != smoothingStates.end() && it->second.revision == entry.state.revision) {
                it->second.elapsed = entry.state.elapsed;
            }
        }
    }
}

void ClientManager::ClearSmoothingState(uint32_t objectId) {
    std::lock_guard<std::mutex> lock(smoothingMutex);
    smoothingStates.erase(objectId);
}

void ClientManager::ClearAllSmoothingStates() {
    std::lock_guard<std::mutex> lock(smoothingMutex);
    smoothingStates.clear();
}

void ClientManager::LoadServerDataConfig() {
    std::ifstream configStream(kServerDataPath);
    if (!configStream.is_open()) {
        std::cerr << "ClientManager: Could not open " << kServerDataPath << ", using default networking parameters." << std::endl;
        return;
    }

    try {
        nlohmann::json configJson;
        configStream >> configJson;

        if (configJson.contains("serverManagerIP") && configJson["serverManagerIP"].is_string()) {
            serverDataConfig.serverManagerIP = configJson["serverManagerIP"].get<std::string>();
        }

        if (configJson.contains("serverManagerPort") && configJson["serverManagerPort"].is_number_unsigned()) {
            uint32_t portValue = configJson["serverManagerPort"].get<uint32_t>();
            if (portValue > 0 && portValue <= std::numeric_limits<uint16_t>::max()) {
                serverDataConfig.serverManagerPort = static_cast<uint16_t>(portValue);
            }
        }

        serverDataConfig.loaded = true;
        std::cout << "ClientManager: Loaded server data config from " << kServerDataPath << std::endl;
        std::cout << "ClientManager: Server Manager at " << serverDataConfig.serverManagerIP 
                 << ":" << serverDataConfig.serverManagerPort << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "ClientManager: Failed to parse " << kServerDataPath << " (" << e.what() << "), using defaults." << std::endl;
    }
}

