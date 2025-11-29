#include "ConnectionManager.h"
#include <enet/enet.h>
#include <iostream>
#include <cstring>
#include <thread>
#include <algorithm>

ConnectionManager::ConnectionManager()
    : enetHost(nullptr)
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

    // ServerManager uses raw UDP, not ENet, so no peer to reset

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
                                     const std::string& hostLocalIP, uint16_t hostLocalPort) {
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

    this->serverManagerIP = serverManagerIP;
    this->serverManagerPort = serverManagerPort;
    this->currentRoomCode = roomCode;

    // Note: ServerManager uses raw UDP (NetworkUtils), not ENet
    // ENet connection is only for game peer-to-peer connections

    // Try connection strategies in order: Direct → Punchthrough → Relay
    connectionStartTime = std::chrono::steady_clock::now();

    // Step 1: Try direct connection
    if (!hostPublicIP.empty() && hostPublicPort > 0) {
        std::cout << "ConnectionManager: Attempting direct connection to " 
                 << hostPublicIP << ":" << hostPublicPort << std::endl;
        if (TryDirectConnection(hostPublicIP, hostPublicPort)) {
            return true;
        }
    }

    // Step 2: Try local IP if public failed
    if (!hostLocalIP.empty() && hostLocalPort > 0 && 
        (hostLocalIP != hostPublicIP || hostLocalPort != hostPublicPort)) {
        std::cout << "ConnectionManager: Attempting direct connection to local " 
                 << hostLocalIP << ":" << hostLocalPort << std::endl;
        if (TryDirectConnection(hostLocalIP, hostLocalPort)) {
            return true;
        }
    }

    // Step 3: Try NAT punchthrough (if enabled)
    // For now, skip and go to relay
    // TODO: Implement NAT punchthrough

    // Step 4: Use relay as fallback
    std::cout << "ConnectionManager: Falling back to relay connection" << std::endl;
    useRelay = true;
    return TryRelayConnection(roomCode);
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

                std::cout << "ConnectionManager: Direct connection established to " << identifier << std::endl;
                return true;
            } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                return false;
            }
        }
    }

    enet_peer_reset(peer);
    return false;
}

bool ConnectionManager::TryRelayConnection(const std::string& roomCode) {
    // Relay connection will be handled via ServerManager using raw UDP
    // For now, mark that we're using relay mode
    // Actual relay forwarding will be implemented in ServerManager later
    
    std::string identifier = "RELAY:" + roomCode;
    PeerConnection peerConn;
    peerConn.peer = nullptr;  // Relay doesn't use ENet peer
    peerConn.identifier = identifier;
    peerConn.type = ConnectionType::RELAY;
    peerConn.lastHeartbeat = std::chrono::steady_clock::now();
    peerConn.connected = true;

    std::lock_guard<std::mutex> lock(peersMutex);
    connectedPeers[identifier] = peerConn;

    std::cout << "ConnectionManager: Relay mode enabled for room " << roomCode 
             << " (packets will go through ServerManager)" << std::endl;
    return true;
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

    // If relay mode, packets will be sent via ServerManager (handled by caller)
    if (it->second.type == ConnectionType::RELAY || !it->second.peer) {
        // Relay mode - return true but actual sending is done via ServerManager
        return true;
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
    std::lock_guard<std::mutex> lock(peersMutex);
    if (connectedPeers.empty()) {
        return false;
    }

    // Get the first connected peer
    auto it = connectedPeers.begin();
    if (!it->second.connected || !it->second.peer) {
        return false;
    }

    // If relay mode, packets will be sent via ServerManager (handled by caller)
    if (it->second.type == ConnectionType::RELAY) {
        return true;
    }

    ENetPacket* packet = enet_packet_create(data, length, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    if (!packet) {
        return false;
    }

    int result = enet_peer_send(it->second.peer, 0, packet);
    if (result < 0) {
        enet_packet_destroy(packet);
        return false;
    }

    bytesSent.fetch_add(length);
    return true;
}

bool ConnectionManager::BroadcastToAllPeers(const void* data, size_t length, bool reliable) {
    if (!enetHost) {
        return false;
    }

    ENetPacket* packet = enet_packet_create(data, length, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    if (!packet) {
        return false;
    }

    std::lock_guard<std::mutex> lock(peersMutex);
    bool sent = false;
    for (auto& [identifier, peerConn] : connectedPeers) {
        if (peerConn.connected && peerConn.peer) {
            // Create a copy of the packet for each peer
            ENetPacket* peerPacket = enet_packet_create(data, length, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
            if (peerPacket) {
                enet_peer_send(peerConn.peer, 0, peerPacket);
                sent = true;
            }
        }
    }

    if (sent) {
        bytesSent.fetch_add(length * connectedPeers.size());
    }
    enet_packet_destroy(packet);
    return sent;
}

bool ConnectionManager::Receive(void* buffer, size_t bufferSize, size_t& received, std::string& fromPeerIdentifier) {
    if (!enetHost) {
        return false;
    }

    ENetEvent event;
    if (enet_host_service(enetHost, &event, 0) <= 0) {
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
    // ServerManager uses raw UDP (NetworkUtils), not ENet
    // This method is a placeholder - actual ServerManager communication
    // is handled by HostManager/ClientManager using NetworkUtils
    return false;
}

bool ConnectionManager::ReceiveFromServerManager(void* buffer, size_t bufferSize, size_t& received) {
    // ServerManager messages are received through normal Receive() method
    // This is a placeholder for future ServerManager-specific message handling
    return false;
}

void ConnectionManager::HandleServerManagerMessage(const void* data, size_t length) {
    // Handle ServerManager-specific messages (relay, punchthrough coordination, etc.)
    // TODO: Implement ServerManager message handling
}

