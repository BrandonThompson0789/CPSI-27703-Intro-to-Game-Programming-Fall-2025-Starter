#include "HostManager.h"
#include "Engine.h"
#include "server_manager/ServerManager.h"
#include "components/BodyComponent.h"
#include "components/SpriteComponent.h"
#include "components/SoundComponent.h"
#include "components/ViewGrabComponent.h"
#include "components/InputComponent.h"
#include "BackgroundManager.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <thread>

HostManager::HostManager(Engine* engine)
    : engine(engine)
    , socket(INVALID_SOCKET_HANDLE)
    , hostPort(8889)
    , serverManagerIP("127.0.0.1")
    , serverManagerPort(8888)
    , roomCode("")
    , isHosting(false)
    , nextObjectId(1)
    , syncInterval(20)  // 20ms
    , serverManagerHeartbeatInterval(5)  // 5 seconds
{
    lastSyncTime = std::chrono::steady_clock::now();
    lastServerManagerHeartbeat = std::chrono::steady_clock::now();
}

HostManager::~HostManager() {
    Shutdown();
}

bool HostManager::Initialize(uint16_t hostPortParam, const std::string& serverManagerIPParam, uint16_t serverManagerPortParam) {
    hostPort = hostPortParam;
    serverManagerIP = serverManagerIPParam;
    serverManagerPort = serverManagerPortParam;

    if (!NetworkUtils::Initialize()) {
        std::cerr << "HostManager: Failed to initialize networking" << std::endl;
        return false;
    }

    socket = NetworkUtils::CreateUDPSocket();
    if (socket == INVALID_SOCKET_HANDLE) {
        std::cerr << "HostManager: Failed to create UDP socket" << std::endl;
        NetworkUtils::Cleanup();
        return false;
    }

    if (!NetworkUtils::BindSocket(socket, "0.0.0.0", hostPort)) {
        std::cerr << "HostManager: Failed to bind socket to port " << hostPort << std::endl;
        NetworkUtils::CloseSocket(socket);
        NetworkUtils::Cleanup();
        return false;
    }

    // Register with Server Manager
    if (!RegisterWithServerManager()) {
        std::cerr << "HostManager: Failed to register with Server Manager" << std::endl;
        NetworkUtils::CloseSocket(socket);
        NetworkUtils::Cleanup();
        return false;
    }

    isHosting = true;
    std::cout << "HostManager: Started hosting on port " << hostPort << " with room code: " << roomCode << std::endl;
    return true;
}

void HostManager::Update(float deltaTime) {
    if (!isHosting) {
        return;
    }

    // Process incoming messages
    ProcessIncomingMessages();

    // Send heartbeat to Server Manager periodically
    auto now = std::chrono::steady_clock::now();
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

    isHosting = false;

    if (socket != INVALID_SOCKET_HANDLE) {
        NetworkUtils::CloseSocket(socket);
        socket = INVALID_SOCKET_HANDLE;
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

    NetworkUtils::Cleanup();
    std::cout << "HostManager: Shutdown complete" << std::endl;
}

bool HostManager::RegisterWithServerManager() {
    HostRegisterMessage msg;
    msg.header.type = MessageType::HOST_REGISTER;
    memset(msg.header.reserved, 0, sizeof(msg.header.reserved));
    msg.hostPort = hostPort;
    msg.reserved[0] = 0;
    msg.reserved[1] = 0;

    // Send registration message
    NetworkUtils::SendTo(socket, &msg, sizeof(msg), serverManagerIP, serverManagerPort);

    // Wait for response (with timeout)
    auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(5);

    char buffer[4096];
    std::string fromIP;
    uint16_t fromPort;

    while (std::chrono::steady_clock::now() - startTime < timeout) {
        int received = NetworkUtils::ReceiveFrom(socket, buffer, sizeof(buffer), fromIP, fromPort);
        
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

    NetworkUtils::SendTo(socket, &msg, sizeof(msg), serverManagerIP, serverManagerPort);
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

    NetworkUtils::SendTo(socket, buffer.data(), buffer.size(), serverManagerIP, serverManagerPort);
}

void HostManager::ProcessIncomingMessages() {
    char buffer[4096];
    std::string fromIP;
    uint16_t fromPort;

    while (true) {
        int received = NetworkUtils::ReceiveFrom(socket, buffer, sizeof(buffer), fromIP, fromPort);
        
        if (received <= 0) {
            break;
        }

        if (received < static_cast<int>(sizeof(HostMessageHeader))) {
            continue;
        }

        const HostMessageHeader* header = reinterpret_cast<const HostMessageHeader*>(buffer);

        switch (header->type) {
            case HostMessageType::CLIENT_CONNECT:
                HandleClientConnect(fromIP, fromPort);
                break;

            case HostMessageType::CLIENT_DISCONNECT:
                HandleClientDisconnect(fromIP, fromPort);
                break;

            case HostMessageType::CLIENT_INPUT:
                if (received >= static_cast<int>(sizeof(ClientInputMessage))) {
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

void HostManager::HandleClientConnect(const std::string& fromIP, uint16_t fromPort) {
    std::string clientKey = fromIP + ":" + std::to_string(fromPort);
    
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        auto it = clients.find(clientKey);
        if (it != clients.end() && it->second.connected) {
            // Client already connected, just send initialization again
            SendInitializationPackage(fromIP, fromPort);
            return;
        }

        // Add new client
        ClientInfo client;
        client.ip = fromIP;
        client.port = fromPort;
        client.lastHeartbeat = std::chrono::steady_clock::now();
        client.connected = true;
        clients[clientKey] = client;
    }

    std::cout << "HostManager: Client connected from " << clientKey << std::endl;
    
    // Send initialization package
    SendInitializationPackage(fromIP, fromPort);
}

void HostManager::HandleClientDisconnect(const std::string& fromIP, uint16_t fromPort) {
    std::string clientKey = fromIP + ":" + std::to_string(fromPort);
    
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        auto it = clients.find(clientKey);
        if (it != clients.end()) {
            it->second.connected = false;
        }
    }

    std::cout << "HostManager: Client disconnected from " << clientKey << std::endl;
}

void HostManager::HandleClientInput(const std::string& fromIP, uint16_t fromPort, const ClientInputMessage& msg) {
    // Find the object by ID
    Object* obj = GetObjectById(msg.objectId);
    if (!obj) {
        return;
    }

    // Get InputComponent
    InputComponent* input = obj->getComponent<InputComponent>();
    if (!input) {
        return;
    }

    // Route network input to InputComponent
    input->setNetworkInput(
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
    std::string clientKey = fromIP + ":" + std::to_string(fromPort);
    
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

void HostManager::SendInitializationPackage(const std::string& clientIP, uint16_t clientPort) {
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

    // Create message
    std::vector<char> buffer(sizeof(InitPackageHeader) + backgroundStr.size() + objectsStr.size() + 2);
    InitPackageHeader* header = reinterpret_cast<InitPackageHeader*>(buffer.data());
    header->header.type = HostMessageType::INIT_PACKAGE;
    memset(header->header.reserved, 0, sizeof(header->header.reserved));
    header->backgroundLayerCount = static_cast<uint32_t>(backgroundJson.is_array() ? backgroundJson.size() : (backgroundJson.empty() ? 0 : 1));
    header->objectCount = static_cast<uint32_t>(objectsArray.size());

    // Append JSON strings (null-terminated)
    char* data = buffer.data() + sizeof(InitPackageHeader);
    memcpy(data, backgroundStr.c_str(), backgroundStr.size());
    data[backgroundStr.size()] = '\0';
    data += backgroundStr.size() + 1;
    memcpy(data, objectsStr.c_str(), objectsStr.size());
    data[objectsStr.size()] = '\0';

    SendToClient(clientIP, clientPort, buffer.data(), buffer.size());
    std::cout << "HostManager: Sent initialization package to " << clientIP << ":" << clientPort << std::endl;
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

        // Serialize component data
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

        // Broadcast to all clients
        BroadcastToAllClients(buffer.data(), buffer.size());
    }
}

void HostManager::SendObjectCreate(Object* obj) {
    if (!obj || obj->isMarkedForDeath()) {
        return;
    }

    uint32_t objectId = GetOrAssignObjectId(obj);
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

    // Remove from ID maps
    {
        std::lock_guard<std::mutex> lock(objectIdsMutex);
        objectToId.erase(obj);
        idToObject.erase(objectId);
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

    for (Object* obj : toRemove) {
        auto it = objectToId.find(obj);
        if (it != objectToId.end()) {
            uint32_t id = it->second;
            idToObject.erase(id);
            objectToId.erase(it);
        }
    }
}

void HostManager::SendToClient(const std::string& clientIP, uint16_t clientPort, const void* data, size_t length) {
    NetworkUtils::SendTo(socket, data, length, clientIP, clientPort);
}

void HostManager::BroadcastToAllClients(const void* data, size_t length) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const auto& [key, client] : clients) {
        if (client.connected) {
            SendToClient(client.ip, client.port, data, length);
        }
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

nlohmann::json HostManager::SerializeObjectForSync(Object* obj) const {
    if (!obj || obj->isMarkedForDeath()) {
        return nlohmann::json();
    }

    nlohmann::json j;
    
    // Serialize name if exists
    if (!obj->getName().empty()) {
        j["name"] = obj->getName();
    }

    // Serialize only the components we need: body, sprite, sound, viewgrab
    nlohmann::json componentsArray = nlohmann::json::array();

    if (obj->hasComponent<BodyComponent>()) {
        componentsArray.push_back(SerializeObjectBody(obj));
    }
    if (obj->hasComponent<SpriteComponent>()) {
        componentsArray.push_back(SerializeObjectSprite(obj));
    }
    if (obj->hasComponent<SoundComponent>()) {
        componentsArray.push_back(SerializeObjectSound(obj));
    }
    if (obj->hasComponent<ViewGrabComponent>()) {
        componentsArray.push_back(SerializeObjectViewGrab(obj));
    }

    j["components"] = componentsArray;
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

