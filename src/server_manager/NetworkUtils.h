#pragma once

#include <string>
#include <cstdint>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET SocketHandle;
    #define INVALID_SOCKET_HANDLE INVALID_SOCKET
    #define SOCKET_ERROR_CODE SOCKET_ERROR
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    typedef int SocketHandle;
    #define INVALID_SOCKET_HANDLE -1
    #define SOCKET_ERROR_CODE -1
    #define CLOSE_SOCKET close
#endif

namespace NetworkUtils {
    // Initialize networking (required on Windows)
    bool Initialize();
    
    // Cleanup networking (required on Windows)
    void Cleanup();
    
    // Create a UDP socket
    SocketHandle CreateUDPSocket();
    
    // Bind socket to address and port
    bool BindSocket(SocketHandle socket, const std::string& address, uint16_t port);
    
    // Send UDP data
    int SendTo(SocketHandle socket, const void* data, size_t length, 
               const std::string& address, uint16_t port);
    
    // Receive UDP data (non-blocking)
    int ReceiveFrom(SocketHandle socket, void* buffer, size_t bufferSize,
                    std::string& fromAddress, uint16_t& fromPort);
    
    // Close socket
    void CloseSocket(SocketHandle socket);
    
    // Get local IP address (returns first non-loopback IPv4 address)
    std::string GetLocalIP();
    
    // Convert string IP to binary format
    bool StringToAddress(const std::string& ip, struct sockaddr_in& addr);
    
    // Convert binary address to string
    std::string AddressToString(const struct sockaddr_in& addr);
    
    // Get public IP address by querying an external service
    // Returns empty string on failure
    std::string GetPublicIP();
}

