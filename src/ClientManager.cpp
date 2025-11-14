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
#include <iostream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <thread>
#include <atomic>

ClientManager::ClientManager(Engine* engine)
    : engine(engine)
    , socket(INVALID_SOCKET_HANDLE)
    , serverManagerIP("127.0.0.1")
    , serverManagerPort(8888)
    , hostIP("")
    , hostPort(0)
    , roomCode("")
    , isConnected(false)
    , controlledObjectId(0)
    , heartbeatInterval(5)  // 5 seconds
    , inputSendInterval(20)  // 20ms
    , bytesSent(0)
    , bytesReceived(0)
{
    lastHeartbeat = std::chrono::steady_clock::now();
    lastInputSend = std::chrono::steady_clock::now();
    lastBandwidthLogTime = std::chrono::steady_clock::now();
}

ClientManager::~ClientManager() {
    Disconnect();
}

bool ClientManager::Connect(const std::string& roomCodeParam, 
                            const std::string& serverManagerIPParam, 
                            uint16_t serverManagerPortParam) {
    roomCode = roomCodeParam;
    serverManagerIP = serverManagerIPParam;
    serverManagerPort = serverManagerPortParam;

    if (!NetworkUtils::Initialize()) {
        std::cerr << "ClientManager: Failed to initialize networking" << std::endl;
        return false;
    }

    socket = NetworkUtils::CreateUDPSocket();
    if (socket == INVALID_SOCKET_HANDLE) {
        std::cerr << "ClientManager: Failed to create UDP socket" << std::endl;
        NetworkUtils::Cleanup();
        return false;
    }

    // Bind to any available port (0 = let OS choose)
    if (!NetworkUtils::BindSocket(socket, "0.0.0.0", 0)) {
        std::cerr << "ClientManager: Failed to bind socket" << std::endl;
        NetworkUtils::CloseSocket(socket);
        NetworkUtils::Cleanup();
        return false;
    }

    // Look up room from Server Manager
    if (!LookupRoom(roomCode, serverManagerIP, serverManagerPort)) {
        std::cerr << "ClientManager: Failed to lookup room: " << roomCode << std::endl;
        NetworkUtils::CloseSocket(socket);
        NetworkUtils::Cleanup();
        return false;
    }

    // Connect to host
    ClientConnectMessage msg;
    msg.header.type = HostMessageType::CLIENT_CONNECT;
    memset(msg.header.reserved, 0, sizeof(msg.header.reserved));
    memset(msg.reserved, 0, sizeof(msg.reserved));

    SendToHost(&msg, sizeof(msg));
    
    // Give the host a moment to process the message
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Wait for initialization package (with timeout)
    auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(10);

    // Retry sending connection message a few times in case it was lost
    int retryCount = 0;
    const int maxRetries = 3;
    auto lastSendTime = std::chrono::steady_clock::now();
    const auto retryInterval = std::chrono::milliseconds(500);
    
    while (std::chrono::steady_clock::now() - startTime < timeout) {
        // Retry sending connection message periodically
        auto now = std::chrono::steady_clock::now();
        if (retryCount < maxRetries && (now - lastSendTime) >= retryInterval) {
            std::cout << "ClientManager: Retrying connection (attempt " << (retryCount + 2) << ")" << std::endl;
            SendToHost(&msg, sizeof(msg));
            lastSendTime = now;
            retryCount++;
        }
        
        ProcessIncomingMessages();
        
        if (isConnected) {
            std::cout << "ClientManager: Connected to host " << hostIP << ":" << hostPort << std::endl;
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cerr << "ClientManager: Timeout waiting for initialization package" << std::endl;
    NetworkUtils::CloseSocket(socket);
    NetworkUtils::Cleanup();
    return false;
}

void ClientManager::Update(float deltaTime) {
    if (!isConnected) {
        return;
    }

    // Process incoming messages
    ProcessIncomingMessages();

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

    // Cleanup object IDs for destroyed objects
    CleanupObjectIds();
}

void ClientManager::Disconnect() {
    if (!isConnected) {
        return;
    }

    // Send disconnect message
    if (socket != INVALID_SOCKET_HANDLE && !hostIP.empty()) {
        ClientConnectMessage msg;
        msg.header.type = HostMessageType::CLIENT_DISCONNECT;
        memset(msg.header.reserved, 0, sizeof(msg.header.reserved));
        memset(msg.reserved, 0, sizeof(msg.reserved));
        SendToHost(&msg, sizeof(msg));
    }

    isConnected = false;

    if (socket != INVALID_SOCKET_HANDLE) {
        NetworkUtils::CloseSocket(socket);
        socket = INVALID_SOCKET_HANDLE;
    }

    {
        std::lock_guard<std::mutex> lock(objectIdsMutex);
        idToObject.clear();
    }

    NetworkUtils::Cleanup();
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

    // Send lookup request
    NetworkUtils::SendTo(socket, &msg, sizeof(msg), serverManagerIPParam, serverManagerPortParam);

    // Wait for response (with timeout)
    auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(5);

    char buffer[4096];
    std::string fromIP;
    uint16_t fromPort;

    while (std::chrono::steady_clock::now() - startTime < timeout) {
        int received = NetworkUtils::ReceiveFrom(socket, buffer, sizeof(buffer), fromIP, fromPort);
        
        if (received >= static_cast<int>(sizeof(RoomInfoResponse))) {
            const RoomInfoResponse* response = reinterpret_cast<const RoomInfoResponse*>(buffer);
            if (response->header.type == MessageType::RESPONSE_ROOM_INFO) {
                hostIP = std::string(response->hostIP);
                hostPort = response->hostPort;
                std::cout << "ClientManager: Found room " << roomCodeParam 
                         << " at " << hostIP << ":" << hostPort << std::endl;
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
    std::string fromIP;
    uint16_t fromPort;

    while (true) {
        int received = NetworkUtils::ReceiveFrom(socket, buffer, sizeof(buffer), fromIP, fromPort);
        
        if (received <= 0) {
            break;
        }

        // Track received bytes
        bytesReceived.fetch_add(received);

        // Verify message is from host
        if (fromIP != hostIP || fromPort != hostPort) {
            continue;
        }

        if (received < static_cast<int>(sizeof(HostMessageHeader))) {
            continue;
        }

        const HostMessageHeader* header = reinterpret_cast<const HostMessageHeader*>(buffer);

        switch (header->type) {
            case HostMessageType::INIT_PACKAGE:
                HandleInitPackage(buffer, received);
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

            case HostMessageType::ASSIGN_CONTROLLED_OBJECT:
                if (received >= static_cast<int>(sizeof(AssignControlledObjectMessage))) {
                    const AssignControlledObjectMessage* msg = reinterpret_cast<const AssignControlledObjectMessage*>(buffer);
                    controlledObjectId = msg->objectId;
                    std::cout << "ClientManager: Assigned to control object ID: " << controlledObjectId << std::endl;
                } else {
                    std::cerr << "ClientManager: ASSIGN_CONTROLLED_OBJECT message too small: " << received << " bytes" << std::endl;
                }
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

        // Set Engine instance
        if (engine) {
            Object::setEngine(engine);
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

    {
        std::lock_guard<std::mutex> lock(objectIdsMutex);
        idToObject.erase(objectId);
    }
}

void ClientManager::SendInput() {
    if (controlledObjectId == 0 || !engine) {
        return;
    }

    // Get input from InputManager for the controlled object
    InputManager& inputManager = InputManager::getInstance();
    
    // Get input values (using keyboard as default, can be configured)
    float moveUp = inputManager.getInputValue(INPUT_SOURCE_KEYBOARD, GameAction::MOVE_UP);
    float moveDown = inputManager.getInputValue(INPUT_SOURCE_KEYBOARD, GameAction::MOVE_DOWN);
    float moveLeft = inputManager.getInputValue(INPUT_SOURCE_KEYBOARD, GameAction::MOVE_LEFT);
    float moveRight = inputManager.getInputValue(INPUT_SOURCE_KEYBOARD, GameAction::MOVE_RIGHT);
    float actionWalk = inputManager.getInputValue(INPUT_SOURCE_KEYBOARD, GameAction::ACTION_WALK);
    float actionInteract = inputManager.getInputValue(INPUT_SOURCE_KEYBOARD, GameAction::ACTION_INTERACT);
    float actionThrow = inputManager.getInputValue(INPUT_SOURCE_KEYBOARD, GameAction::ACTION_THROW);

    // Send input message
    ClientInputMessage msg;
    msg.header.type = HostMessageType::CLIENT_INPUT;
    memset(msg.header.reserved, 0, sizeof(msg.header.reserved));
    msg.objectId = controlledObjectId;
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
    if (socket != INVALID_SOCKET_HANDLE && !hostIP.empty() && hostPort > 0) {
        int result = NetworkUtils::SendTo(socket, data, length, hostIP, hostPort);
        if (result > 0) {
            // Track sent bytes
            bytesSent.fetch_add(result);
        }
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

