#include "ConnectionManager.h"
#include "server_manager/ServerManager.h"
#include <enet/enet.h>
#include <iostream>
#include <cstring>
#include <thread>
#include <algorithm>
#include <limits>

ConnectionManager::ConnectionManager()
    : enetHost(nullptr)
    , serverManagerSocket(INVALID_SOCKET_HANDLE)
    , initialized(false)
    , isHosting(false)
    , useRelay(false)
    , serverManagerPort(0)
    , bytesSent(0)
    , bytesReceived(0)
{
}

ConnectionManager::~ConnectionManager() {
    Cleanup();
}

bool ConnectionManager::Initialize() {
    if (initialized) {
        return true;
    }

    if (enet_initialize() != 0) {
        std::cerr << "ConnectionManager: Failed to initialize ENet" << std::endl;
        return false;
    }

    initialized = true;
    return true;
}

void ConnectionManager::Cleanup() {
    if (!initialized) {
        return;
    }

    DisconnectFromHost();
    StopHost();

    // Close ServerManager socket if open
    if (serverManagerSocket != INVALID_SOCKET_HANDLE) {
        NetworkUtils::CloseSocket(serverManagerSocket);
        serverManagerSocket = INVALID_SOCKET_HANDLE;
    }

    enet_deinitialize();
    initialized = false;
}

bool ConnectionManager::StartHost(uint16_t port, uint32_t maxClients) {
    if (!initialized) {
        if (!Initialize()) {
            return false;
        }
    }

    if (isHosting) {
        StopHost();
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    enetHost = enet_host_create(&address, maxClients, 2, 0, 0);
    if (!enetHost) {
        std::cerr << "ConnectionManager: Failed to create host on port " << port << std::endl;
        return false;
    }

    isHosting = true;
    std::cout << "ConnectionManager: Started hosting on port " << port << std::endl;
    return true;
}

void ConnectionManager::StopHost() {
    if (!isHosting || !enetHost) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(peersMutex);
        for (auto& [identifier, peerConn] : connectedPeers) {
            if (peerConn.peer) {
                enet_peer_disconnect(peerConn.peer, 0);
            }
        }
        connectedPeers.clear();
    }

    // Give peers time to receive disconnect message
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    enet_host_flush(enetHost);
    enet_host_destroy(enetHost);
    enetHost = nullptr;
    isHosting = false;

    std::cout << "ConnectionManager: Stopped hosting" << std::endl;
}

bool ConnectionManager::ConnectToHost(const std::string& roomCode,
                                     const std::string& serverManagerIP, uint16_t serverManagerPort,
                                     const std::string& hostPublicIP, uint16_t hostPublicPort,
                                     const std::string& hostLocalIP, uint16_t hostLocalPort,
                                     uint8_t forcedConnectionType, bool relayEnabled) {
    if (!initialized) {
        if (!Initialize()) {
            return false;
        }
    }

    // Create client host if not already created
    if (!enetHost) {
        enetHost = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!enetHost) {
            std::cerr << "ConnectionManager: Failed to create client host" << std::endl;
            return false;
        }
    }

    // Initialize ServerManager socket if needed
    if (serverManagerSocket == INVALID_SOCKET_HANDLE) {
        if (!NetworkUtils::Initialize()) {
            std::cerr << "ConnectionManager: Failed to initialize NetworkUtils" << std::endl;
            return false;
        }
        serverManagerSocket = NetworkUtils::CreateUDPSocket();
        if (serverManagerSocket == INVALID_SOCKET_HANDLE) {
            std::cerr << "ConnectionManager: Failed to create ServerManager socket" << std::endl;
            return false;
        }
    }

    this->serverManagerIP = serverManagerIP;
    this->serverManagerPort = serverManagerPort;
    this->currentRoomCode = roomCode;

    // Measure path efficiency to determine best connection order
    std::vector<PathInfo> paths = MeasurePathEfficiency(hostPublicIP, hostPublicPort,
                                                         hostLocalIP, hostLocalPort);
    
    // Sort paths by latency (most efficient first)
    std::sort(paths.begin(), paths.end(), 
              [](const PathInfo& a, const PathInfo& b) {
                  if (a.reachable != b.reachable) return a.reachable;
                  return a.latencyMs < b.latencyMs;
              });

    connectionStartTime = std::chrono::steady_clock::now();

    // Check forced connection type from server
    // forcedConnectionType: 0 = NONE, 1 = DIRECT_ONLY, 2 = NAT_ONLY, 3 = RELAY_ONLY
    enum class ForcedConnectionType {
        NONE = 0,
        DIRECT_ONLY = 1,
        NAT_ONLY = 2,
        RELAY_ONLY = 3
    };
    ForcedConnectionType forcedType = static_cast<ForcedConnectionType>(forcedConnectionType);
    
    // Step 1: Try direct connections (if not forced to skip)
    if (forcedType != ForcedConnectionType::NAT_ONLY && forcedType != ForcedConnectionType::RELAY_ONLY) {
        for (const auto& path : paths) {
            if (path.reachable) {
                std::cout << "ConnectionManager: Attempting direct connection to " 
                         << path.ip << ":" << path.port 
                         << " (latency: " << path.latencyMs << "ms)" << std::endl;
                if (TryDirectConnection(path.ip, path.port)) {
                    return true;
                }
            }
        }
    } else if (forcedType == ForcedConnectionType::DIRECT_ONLY) {
        // Direct only - if we get here, direct failed, so return false
        std::cerr << "ConnectionManager: Direct connection required but failed" << std::endl;
        return false;
    }

    // Step 2: Try NAT punchthrough (if not forced to skip)
    if (forcedType != ForcedConnectionType::DIRECT_ONLY && forcedType != ForcedConnectionType::RELAY_ONLY) {
        std::cout << "ConnectionManager: Attempting NAT punchthrough" << std::endl;
        if (TryNATPunchthrough(roomCode, hostPublicIP, hostPublicPort, hostLocalIP, hostLocalPort)) {
            return true;
        }
    } else if (forcedType == ForcedConnectionType::NAT_ONLY) {
        // NAT only - if we get here, NAT failed, so return false
        std::cerr << "ConnectionManager: NAT punchthrough required but failed" << std::endl;
        return false;
    }

    // Step 3: Use relay as fallback (if not forced to skip and relay is enabled)
    if (forcedType != ForcedConnectionType::DIRECT_ONLY && forcedType != ForcedConnectionType::NAT_ONLY) {
        if (relayEnabled) {
            std::cout << "ConnectionManager: Falling back to relay connection" << std::endl;
            useRelay = true;
            return TryRelayConnection(roomCode);
        } else {
            std::cerr << "ConnectionManager: Relay required but disabled on server" << std::endl;
            return false;
        }
    } else if (forcedType == ForcedConnectionType::RELAY_ONLY) {
        // Relay only - if we get here, relay failed, so return false
        std::cerr << "ConnectionManager: Relay connection required but failed" << std::endl;
        return false;
    }

    return false;
}

bool ConnectionManager::TryDirectConnection(const std::string& hostIP, uint16_t hostPort) {
    ENetAddress address;
    if (enet_address_set_host(&address, hostIP.c_str()) != 0) {
        return false;
    }
    address.port = hostPort;

    ENetPeer* peer = enet_host_connect(enetHost, &address, 2, 0);
    if (!peer) {
        return false;
    }

    // Wait for connection (with timeout)
    ENetEvent event;
    auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(2);

    while (std::chrono::steady_clock::now() - startTime < timeout) {
        if (enet_host_service(enetHost, &event, 100) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                std::string identifier = hostIP + ":" + std::to_string(hostPort);
                PeerConnection peerConn;
                peerConn.peer = peer;
                peerConn.identifier = identifier;
                peerConn.type = ConnectionType::DIRECT;
                peerConn.lastHeartbeat = std::chrono::steady_clock::now();
                peerConn.connected = true;

                std::lock_guard<std::mutex> lock(peersMutex);
                connectedPeers[identifier] = peerConn;

                std::cout << "ConnectionManager: Direct connection established to " << identifier << " (Connection Type: DIRECT)" << std::endl;
                return true;
            } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                return false;
            }
        }
    }

    enet_peer_reset(peer);
    return false;
}

bool ConnectionManager::TryNATPunchthrough(const std::string& roomCode,
                           const std::string& hostPublicIP, uint16_t hostPublicPort,
                           const std::string& hostLocalIP, uint16_t hostLocalPort) {
    // Request NAT punchthrough coordination from ServerManager
    NATPunchthroughRequest request;
    request.header.type = MessageType::NAT_PUNCHTHROUGH_REQUEST;
    memset(request.header.reserved, 0, sizeof(request.header.reserved));
    strncpy(request.roomCode, roomCode.c_str(), sizeof(request.roomCode) - 1);
    request.roomCode[sizeof(request.roomCode) - 1] = '\0';
    
    // Get local IP
    std::string localIP = NetworkUtils::GetLocalIP();
    strncpy(request.clientLocalIP, localIP.c_str(), sizeof(request.clientLocalIP) - 1);
    request.clientLocalIP[sizeof(request.clientLocalIP) - 1] = '\0';
    request.clientLocalPort = 0;  // Will be detected by server manager
    
    // Send request
    if (!SendToServerManager(&request, sizeof(request))) {
        return false;
    }

    // Wait for punchthrough response
    auto startTime = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - startTime < PUNCHTHROUGH_TIMEOUT) {
        char buffer[4096];
        size_t received = 0;
        if (ReceiveFromServerManager(buffer, sizeof(buffer), received)) {
            HandleServerManagerMessage(buffer, received);
            
            // Check if we received punchthrough response
            if (received >= sizeof(NATPunchthroughResponse)) {
                const NATPunchthroughResponse* response = 
                    reinterpret_cast<const NATPunchthroughResponse*>(buffer);
                if (response->header.type == MessageType::NAT_PUNCHTHROUGH_RESPONSE) {
                    // Try connecting to both public and local addresses simultaneously
                    // This is the "punch" - both sides send packets to each other
                    std::string hostPub = response->hostPublicIP;
                    uint16_t hostPubPort = response->hostPublicPort;
                    std::string hostLoc = response->hostLocalIP;
                    uint16_t hostLocPort = response->hostLocalPort;
                    
                    // Try public first
                    if (TryDirectConnection(hostPub, hostPubPort)) {
                        std::string identifier = hostPub + ":" + std::to_string(hostPubPort);
                        std::lock_guard<std::mutex> lock(peersMutex);
                        auto it = connectedPeers.find(identifier);
                        if (it != connectedPeers.end()) {
                            it->second.type = ConnectionType::NAT_PUNCHTHROUGH;
                        }
                        std::cout << "ConnectionManager: NAT punchthrough successful via public IP (Connection Type: NAT_PUNCHTHROUGH)" << std::endl;
                        return true;
                    }
                    
                    // Try local
                    if (TryDirectConnection(hostLoc, hostLocPort)) {
                        std::string identifier = hostLoc + ":" + std::to_string(hostLocPort);
                        std::lock_guard<std::mutex> lock(peersMutex);
                        auto it = connectedPeers.find(identifier);
                        if (it != connectedPeers.end()) {
                            it->second.type = ConnectionType::NAT_PUNCHTHROUGH;
                        }
                        std::cout << "ConnectionManager: NAT punchthrough successful via local IP (Connection Type: NAT_PUNCHTHROUGH)" << std::endl;
                        return true;
                    }
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return false;
}

bool ConnectionManager::TryRelayConnection(const std::string& roomCode) {
    // Request relay connection from ServerManager
    RelayRequest request;
    request.header.type = MessageType::RELAY_REQUEST;
    memset(request.header.reserved, 0, sizeof(request.header.reserved));
    strncpy(request.roomCode, roomCode.c_str(), sizeof(request.roomCode) - 1);
    request.roomCode[sizeof(request.roomCode) - 1] = '\0';
    
    std::string localIP = NetworkUtils::GetLocalIP();
    strncpy(request.clientIP, localIP.c_str(), sizeof(request.clientIP) - 1);
    request.clientIP[sizeof(request.clientIP) - 1] = '\0';
    request.clientPort = 0;  // Will be detected by server manager
    
    // Send request
    if (!SendToServerManager(&request, sizeof(request))) {
        return false;
    }

    // Wait for relay response
    auto startTime = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - startTime < RELAY_TIMEOUT) {
        char buffer[4096];
        size_t received = 0;
        if (ReceiveFromServerManager(buffer, sizeof(buffer), received)) {
            HandleServerManagerMessage(buffer, received);
            
            if (received >= sizeof(RelayResponse)) {
                const RelayResponse* response = 
                    reinterpret_cast<const RelayResponse*>(buffer);
                if (response->header.type == MessageType::RELAY_RESPONSE) {
                    if (response->accepted) {
                        std::string identifier = "RELAY:" + roomCode;
                        PeerConnection peerConn;
                        peerConn.peer = nullptr;  // Relay doesn't use ENet peer
                        peerConn.identifier = identifier;
                        peerConn.type = ConnectionType::RELAY;
                        peerConn.lastHeartbeat = std::chrono::steady_clock::now();
                        peerConn.connected = true;

                        std::lock_guard<std::mutex> lock(peersMutex);
                        connectedPeers[identifier] = peerConn;

                        std::cout << "ConnectionManager: Relay connection established for room " 
                                 << roomCode << " (Connection Type: RELAY)" << std::endl;
                        return true;
                    } else {
                        std::cerr << "ConnectionManager: Relay request declined by server" << std::endl;
                        return false;
                    }
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return false;
}

void ConnectionManager::DisconnectFromHost() {
    std::lock_guard<std::mutex> lock(peersMutex);
    for (auto& [identifier, peerConn] : connectedPeers) {
        if (peerConn.peer) {
            enet_peer_disconnect(peerConn.peer, 0);
        }
    }
    connectedPeers.clear();
}

bool ConnectionManager::SendToPeer(const std::string& peerIdentifier, const void* data, size_t length, bool reliable) {
    std::lock_guard<std::mutex> lock(peersMutex);
    auto it = connectedPeers.find(peerIdentifier);
    if (it == connectedPeers.end() || !it->second.connected) {
        // Debug: list available peers
        std::cerr << "ConnectionManager: Peer not found: " << peerIdentifier << std::endl;
        std::cerr << "ConnectionManager: Available peers: ";
        for (const auto& [id, peer] : connectedPeers) {
            std::cerr << id << " ";
        }
        std::cerr << std::endl;
        return false;
    }

    // If relay mode, send via ServerManager
    if (it->second.type == ConnectionType::RELAY || !it->second.peer) {
        // Send through relay
        // Extract room code from peer identifier (format: "RELAY:ROOMCODE")
        std::string roomCode;
        if (peerIdentifier.find("RELAY:") == 0) {
            roomCode = peerIdentifier.substr(6);  // Skip "RELAY:"
        } else {
            // Fallback to currentRoomCode if identifier doesn't match expected format
            roomCode = currentRoomCode;
        }
        
        RelayDataHeader header;
        header.header.type = MessageType::RELAY_DATA;
        memset(header.header.reserved, 0, sizeof(header.header.reserved));
        strncpy(header.roomCode, roomCode.c_str(), sizeof(header.roomCode) - 1);
        header.roomCode[sizeof(header.roomCode) - 1] = '\0';
        
        // Extract destination from peer identifier (RELAY:ROOMCODE format)
        // For relay, we need to get host info from ServerManager
        // For now, use room code to identify destination
        std::string localIP = NetworkUtils::GetLocalIP();
        strncpy(header.fromIP, localIP.c_str(), sizeof(header.fromIP) - 1);
        header.fromIP[sizeof(header.fromIP) - 1] = '\0';
        header.fromPort = 0;
        
        // Destination will be determined by ServerManager based on room code
        strncpy(header.toIP, "", sizeof(header.toIP) - 1);
        header.toPort = 0;
        header.dataLength = static_cast<uint32_t>(length);
        
        // Create message with header + data
        std::vector<char> message(sizeof(header) + length);
        memcpy(message.data(), &header, sizeof(header));
        memcpy(message.data() + sizeof(header), data, length);
        
        return SendToServerManager(message.data(), message.size());
    }

    ENetPacket* packet = enet_packet_create(data, length, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    if (!packet) {
        std::cerr << "ConnectionManager: Failed to create packet" << std::endl;
        return false;
    }

    int result = enet_peer_send(it->second.peer, 0, packet);
    if (result < 0) {
        std::cerr << "ConnectionManager: Failed to send packet to peer: " << result << std::endl;
        enet_packet_destroy(packet);
        return false;
    }

    bytesSent.fetch_add(length);
    return true;
}

bool ConnectionManager::SendToFirstPeer(const void* data, size_t length, bool reliable) {
    std::string identifier;
    {
        std::lock_guard<std::mutex> lock(peersMutex);
        if (connectedPeers.empty()) {
            return false;
        }

        // Get the first connected peer
        auto it = connectedPeers.begin();
        if (!it->second.connected) {
            return false;
        }

        identifier = it->second.identifier;
    }
    // Use SendToPeer which handles relay mode
    return SendToPeer(identifier, data, length, reliable);
}

bool ConnectionManager::BroadcastToAllPeers(const void* data, size_t length, bool reliable) {
    // Collect all connected peer identifiers first (to avoid holding lock during SendToPeer)
    std::vector<std::string> peerIdentifiers;
    {
        std::lock_guard<std::mutex> lock(peersMutex);
        for (auto& [identifier, peerConn] : connectedPeers) {
            if (peerConn.connected) {
                peerIdentifiers.push_back(identifier);
            }
        }
    }
    
    // Send to each peer (handles both ENet and relay connections)
    // SendToPeer will acquire its own lock
    bool sent = false;
    for (const auto& identifier : peerIdentifiers) {
        if (SendToPeer(identifier, data, length, reliable)) {
            sent = true;
        }
    }
    
    return sent;
}

bool ConnectionManager::Receive(void* buffer, size_t bufferSize, size_t& received, std::string& fromPeerIdentifier) {
    if (!enetHost) {
        // Even if no ENet host, check for relay data
        if (serverManagerSocket != INVALID_SOCKET_HANDLE) {
            char relayBuffer[4096];
            size_t relayReceived = 0;
            if (ReceiveFromServerManager(relayBuffer, sizeof(relayBuffer), relayReceived)) {
                if (relayReceived >= sizeof(RelayDataHeader)) {
                    const RelayDataHeader* relayHeader = 
                        reinterpret_cast<const RelayDataHeader*>(relayBuffer);
                    if (relayHeader->header.type == MessageType::RELAY_DATA) {
                        size_t dataSize = relayReceived - sizeof(RelayDataHeader);
                        if (dataSize > bufferSize) {
                            dataSize = bufferSize;
                        }
                        memcpy(buffer, relayBuffer + sizeof(RelayDataHeader), dataSize);
                        received = dataSize;
                        fromPeerIdentifier = "RELAY:" + std::string(relayHeader->roomCode);
                        bytesReceived.fetch_add(received);
                        return true;
                    }
                }
            }
        }
        return false;
    }

    ENetEvent event;
    if (enet_host_service(enetHost, &event, 0) <= 0) {
        // No ENet event, check for relay data (process multiple messages to drain queue)
        if (serverManagerSocket != INVALID_SOCKET_HANDLE) {
            // Process up to 10 relay messages per call to avoid blocking
            for (int i = 0; i < 10; ++i) {
                char relayBuffer[4096];
                size_t relayReceived = 0;
                if (ReceiveFromServerManager(relayBuffer, sizeof(relayBuffer), relayReceived)) {
                    if (relayReceived >= sizeof(RelayDataHeader)) {
                        const RelayDataHeader* relayHeader = 
                            reinterpret_cast<const RelayDataHeader*>(relayBuffer);
                        if (relayHeader->header.type == MessageType::RELAY_DATA) {
                            size_t dataSize = relayReceived - sizeof(RelayDataHeader);
                            if (dataSize > bufferSize) {
                                dataSize = bufferSize;
                            }
                            memcpy(buffer, relayBuffer + sizeof(RelayDataHeader), dataSize);
                            received = dataSize;
                            fromPeerIdentifier = "RELAY:" + std::string(relayHeader->roomCode);
                            bytesReceived.fetch_add(received);
                            return true;
                        }
                    }
                } else {
                    // No more relay messages available
                    break;
                }
            }
        }
        return false;
    }

    switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            // New peer connected (when hosting)
            std::string identifier;
            if (event.peer->address.host != 0) {
                char hostStr[256];
                enet_address_get_host_ip(&event.peer->address, hostStr, sizeof(hostStr));
                identifier = std::string(hostStr) + ":" + std::to_string(event.peer->address.port);
            } else {
                identifier = "UNKNOWN:" + std::to_string(event.peer->address.port);
            }

            PeerConnection peerConn;
            peerConn.peer = event.peer;
            peerConn.identifier = identifier;
            peerConn.type = ConnectionType::DIRECT;
            peerConn.lastHeartbeat = std::chrono::steady_clock::now();
            peerConn.connected = true;

            std::lock_guard<std::mutex> lock(peersMutex);
            connectedPeers[identifier] = peerConn;

            return false;  // No data to receive, just connection event
        }

        case ENET_EVENT_TYPE_DISCONNECT: {
            // Peer disconnected
            std::lock_guard<std::mutex> lock(peersMutex);
            for (auto it = connectedPeers.begin(); it != connectedPeers.end(); ++it) {
                if (it->second.peer == event.peer) {
                    std::cout << "ConnectionManager: Peer disconnected: " << it->second.identifier << std::endl;
                    connectedPeers.erase(it);
                    break;
                }
            }
            return false;  // No data to receive, just disconnect event
        }

        case ENET_EVENT_TYPE_RECEIVE: {
            // Data received
            size_t dataSize = event.packet->dataLength;
            if (dataSize > bufferSize) {
                dataSize = bufferSize;
            }
            memcpy(buffer, event.packet->data, dataSize);
            received = dataSize;

            bytesReceived.fetch_add(received);

            // Find peer identifier
            {
                std::lock_guard<std::mutex> lock(peersMutex);
                for (const auto& [identifier, peerConn] : connectedPeers) {
                    if (peerConn.peer == event.peer) {
                        fromPeerIdentifier = identifier;
                        break;
                    }
                }
            }

            // If peer not found in connectedPeers (shouldn't happen, but handle gracefully)
            if (fromPeerIdentifier.empty() && event.peer) {
                char hostStr[256];
                enet_address_get_host_ip(&event.peer->address, hostStr, sizeof(hostStr));
                fromPeerIdentifier = std::string(hostStr) + ":" + std::to_string(event.peer->address.port);
                
                // Add to connectedPeers if not already there (race condition protection)
                std::lock_guard<std::mutex> lock(peersMutex);
                if (connectedPeers.find(fromPeerIdentifier) == connectedPeers.end()) {
                    PeerConnection peerConn;
                    peerConn.peer = event.peer;
                    peerConn.identifier = fromPeerIdentifier;
                    peerConn.type = ConnectionType::DIRECT;
                    peerConn.lastHeartbeat = std::chrono::steady_clock::now();
                    peerConn.connected = true;
                    connectedPeers[fromPeerIdentifier] = peerConn;
                    std::cout << "ConnectionManager: Added peer from RECEIVE event: " << fromPeerIdentifier << std::endl;
                }
            }

            enet_packet_destroy(event.packet);
            return true;
        }
        
        default:
            return false;
    }
    
    // Also check for relay data from ServerManager
    if (serverManagerSocket != INVALID_SOCKET_HANDLE) {
        char relayBuffer[4096];
        size_t relayReceived = 0;
        if (ReceiveFromServerManager(relayBuffer, sizeof(relayBuffer), relayReceived)) {
            if (relayReceived >= sizeof(RelayDataHeader)) {
                const RelayDataHeader* relayHeader = 
                    reinterpret_cast<const RelayDataHeader*>(relayBuffer);
                if (relayHeader->header.type == MessageType::RELAY_DATA) {
                    // Extract actual data (skip header)
                    size_t dataSize = relayReceived - sizeof(RelayDataHeader);
                    if (dataSize > bufferSize) {
                        dataSize = bufferSize;
                    }
                    memcpy(buffer, relayBuffer + sizeof(RelayDataHeader), dataSize);
                    received = dataSize;
                    fromPeerIdentifier = "RELAY:" + std::string(relayHeader->roomCode);
                    bytesReceived.fetch_add(received);
                    return true;
                }
            }
        }
    }
    
    return false;
}

void ConnectionManager::Update(float deltaTime) {
    if (!enetHost) {
        return;
    }

    // Don't process events here - they're handled in Receive()
    // ProcessENetEvents() was consuming events before Receive() could see them
    // Events are now processed only in Receive() method
    CleanupDisconnectedPeers();
}

void ConnectionManager::Flush() {
    if (enetHost) {
        enet_host_flush(enetHost);
    }
}

ConnectionType ConnectionManager::GetConnectionType(const std::string& peerIdentifier) const {
    std::lock_guard<std::mutex> lock(peersMutex);
    auto it = connectedPeers.find(peerIdentifier);
    if (it != connectedPeers.end()) {
        return it->second.type;
    }
    return ConnectionType::NONE;
}

size_t ConnectionManager::GetConnectedPeerCount() const {
    std::lock_guard<std::mutex> lock(peersMutex);
    return connectedPeers.size();
}

std::string ConnectionManager::GetFirstConnectedPeerIdentifier() const {
    std::lock_guard<std::mutex> lock(peersMutex);
    if (!connectedPeers.empty()) {
        // Return the first connected peer identifier
        return connectedPeers.begin()->first;
    }
    return "";
}

void ConnectionManager::SetUseRelay(bool useRelay) {
    this->useRelay = useRelay;
}

std::string ConnectionManager::GetLocalIP() const {
    // ENet doesn't provide direct IP address access
    // Return empty string for now (can be enhanced if needed)
    return "";
}

std::string ConnectionManager::GetPublicIP() {
    // TODO: Implement public IP detection
    // For now, return empty string
    return "";
}

void ConnectionManager::SetServerManagerSocket(SocketHandle socket, const std::string& serverManagerIP, uint16_t serverManagerPort) {
    this->serverManagerSocket = socket;
    this->serverManagerIP = serverManagerIP;
    this->serverManagerPort = serverManagerPort;
}

bool ConnectionManager::RegisterRelayPeer(const std::string& roomCode) {
    std::string identifier = "RELAY:" + roomCode;
    
    std::lock_guard<std::mutex> lock(peersMutex);
    
    // Check if already registered
    auto it = connectedPeers.find(identifier);
    if (it != connectedPeers.end() && it->second.connected) {
        return true;  // Already registered
    }
    
    // Register the relay peer
    PeerConnection peerConn;
    peerConn.peer = nullptr;  // Relay doesn't use ENet peer
    peerConn.identifier = identifier;
    peerConn.type = ConnectionType::RELAY;
    peerConn.lastHeartbeat = std::chrono::steady_clock::now();
    peerConn.connected = true;
    
    connectedPeers[identifier] = peerConn;
    
    std::cout << "ConnectionManager: Registered relay peer for room " << roomCode << std::endl;
    return true;
}

std::string ConnectionManager::GetNewPeerConnection() {
    // Connection events are handled directly in Receive()
    // This method is kept for API compatibility but returns empty
    // since connections are processed immediately
    return "";
}

std::string ConnectionManager::GetDisconnectedPeer() {
    // Disconnection events are handled directly in Receive()
    // This method is kept for API compatibility but returns empty
    // since disconnections are processed immediately
    return "";
}

void ConnectionManager::ProcessENetEvents() {
    if (!enetHost) {
        return;
    }

    ENetEvent event;
    while (enet_host_service(enetHost, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                // Handle connection (already handled in Receive)
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                // Handle disconnection (already handled in Receive)
                break;

            case ENET_EVENT_TYPE_RECEIVE:
                // Data will be handled in Receive() method
                // Store event for later processing if needed
                break;

            default:
                break;
        }
    }
}

void ConnectionManager::CleanupDisconnectedPeers() {
    // ENet handles disconnections automatically
    // This can be used for custom cleanup logic if needed
}

ENetPeer* ConnectionManager::FindOrCreatePeer(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(peersMutex);
    auto it = connectedPeers.find(identifier);
    if (it != connectedPeers.end()) {
        return it->second.peer;
    }
    return nullptr;
}

void ConnectionManager::RemovePeer(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(peersMutex);
    auto it = connectedPeers.find(identifier);
    if (it != connectedPeers.end()) {
        if (it->second.peer) {
            enet_peer_reset(it->second.peer);
        }
        connectedPeers.erase(it);
    }
}

bool ConnectionManager::SendToServerManager(const void* data, size_t length) {
    if (serverManagerSocket == INVALID_SOCKET_HANDLE) {
        return false;
    }
    int result = NetworkUtils::SendTo(serverManagerSocket, data, length, serverManagerIP, serverManagerPort);
    return result > 0;
}

bool ConnectionManager::ReceiveFromServerManager(void* buffer, size_t bufferSize, size_t& received) {
    if (serverManagerSocket == INVALID_SOCKET_HANDLE) {
        return false;
    }
    std::string fromIP;
    uint16_t fromPort;
    int result = NetworkUtils::ReceiveFrom(serverManagerSocket, buffer, bufferSize, fromIP, fromPort);
    if (result > 0 && fromIP == serverManagerIP && fromPort == serverManagerPort) {
        received = static_cast<size_t>(result);
        return true;
    }
    return false;
}

void ConnectionManager::HandleServerManagerMessage(const void* data, size_t length) {
    if (length < sizeof(MessageHeader)) {
        return;
    }
    
    const MessageHeader* header = 
        reinterpret_cast<const MessageHeader*>(data);
    
    // Messages are handled in the calling context (TryNATPunchthrough, TryRelayConnection)
    // This method can be extended for async message handling if needed
}

std::vector<ConnectionManager::PathInfo> ConnectionManager::MeasurePathEfficiency(
    const std::string& hostPublicIP, uint16_t hostPublicPort,
    const std::string& hostLocalIP, uint16_t hostLocalPort) {
    
    std::vector<PathInfo> paths;
    
    // Check cache
    auto now = std::chrono::steady_clock::now();
    if (!cachedPaths.empty() && (now - pathCacheTime) < PATH_CACHE_DURATION) {
        return cachedPaths;
    }
    
    // For now, we'll assume both paths are potentially reachable
    // In a production system, you might want to do a lightweight ping test
    // without actually establishing a full connection
    
    // Add public IP path
    if (!hostPublicIP.empty() && hostPublicPort > 0) {
        PathInfo path;
        path.ip = hostPublicIP;
        path.port = hostPublicPort;
        path.reachable = true;  // Assume reachable, will be tested during actual connection
        path.latencyMs = 0;  // Will be measured during connection attempt
        paths.push_back(path);
    }
    
    // Add local IP path
    if (!hostLocalIP.empty() && hostLocalPort > 0 && 
        (hostLocalIP != hostPublicIP || hostLocalPort != hostPublicPort)) {
        PathInfo path;
        path.ip = hostLocalIP;
        path.port = hostLocalPort;
        path.reachable = true;  // Local IP is typically more reliable
        path.latencyMs = 0;  // Local connections are typically faster
        paths.push_back(path);
    }
    
    // Prefer local IP if available (lower latency expected)
    if (paths.size() > 1) {
        // Sort: local first, then public
        std::sort(paths.begin(), paths.end(),
                  [&hostLocalIP](const PathInfo& a, const PathInfo& b) {
                      // Prefer local IP
                      if (a.ip == hostLocalIP && b.ip != hostLocalIP) return true;
                      if (a.ip != hostLocalIP && b.ip == hostLocalIP) return false;
                      return a.latencyMs < b.latencyMs;
                  });
    }
    
    // Cache results
    cachedPaths = paths;
    pathCacheTime = now;
    
    return paths;
}

