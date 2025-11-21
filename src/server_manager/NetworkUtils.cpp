#include "NetworkUtils.h"
#include <cstring>
#include <iostream>
#include <string>

#ifndef _WIN32
    #include <fcntl.h>
    #include <ifaddrs.h>
    #include <sys/time.h>
#endif

#ifdef _WIN32
    #include <ws2tcpip.h>
#else
    #include <netdb.h>
    #include <unistd.h>
#endif

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
#endif

namespace NetworkUtils {
    static bool g_initialized = false;

    bool Initialize() {
        if (g_initialized) {
            return true;
        }

#ifdef _WIN32
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << std::endl;
            return false;
        }
#endif
        g_initialized = true;
        return true;
    }

    void Cleanup() {
        if (!g_initialized) {
            return;
        }

#ifdef _WIN32
        WSACleanup();
#endif
        g_initialized = false;
    }

    SocketHandle CreateUDPSocket() {
        SocketHandle sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET_HANDLE) {
            return INVALID_SOCKET_HANDLE;
        }

        // Set socket to non-blocking
#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

        // Allow address reuse
        int reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        return sock;
    }

    bool BindSocket(SocketHandle socket, const std::string& address, uint16_t port) {
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (address.empty() || address == "0.0.0.0") {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
                return false;
            }
        }

        if (bind(socket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR_CODE) {
            return false;
        }

        return true;
    }

    int SendTo(SocketHandle socket, const void* data, size_t length,
               const std::string& address, uint16_t port) {
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
            return SOCKET_ERROR_CODE;
        }

        return sendto(socket, reinterpret_cast<const char*>(data), 
                     static_cast<int>(length), 0,
                     reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    }

    int ReceiveFrom(SocketHandle socket, void* buffer, size_t bufferSize,
                    std::string& fromAddress, uint16_t& fromPort) {
        struct sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        std::memset(&addr, 0, sizeof(addr));

        int received = recvfrom(socket, reinterpret_cast<char*>(buffer),
                               static_cast<int>(bufferSize), 0,
                               reinterpret_cast<struct sockaddr*>(&addr), &addrLen);

        if (received > 0) {
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr.sin_addr, ipStr, INET_ADDRSTRLEN);
            fromAddress = ipStr;
            fromPort = ntohs(addr.sin_port);
        }

        return received;
    }

    void CloseSocket(SocketHandle socket) {
        if (socket != INVALID_SOCKET_HANDLE) {
            CLOSE_SOCKET(socket);
        }
    }

    std::string GetLocalIP() {
#ifdef _WIN32
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR_CODE) {
            return "127.0.0.1";
        }

        struct addrinfo hints, *result = nullptr;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) {
            return "127.0.0.1";
        }

        for (struct addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
            if (ptr->ai_family == AF_INET) {
                struct sockaddr_in* sockaddr = reinterpret_cast<struct sockaddr_in*>(ptr->ai_addr);
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sockaddr->sin_addr, ipStr, INET_ADDRSTRLEN);
                
                // Skip loopback addresses
                if (std::string(ipStr) != "127.0.0.1") {
                    freeaddrinfo(result);
                    return ipStr;
                }
            }
        }

        freeaddrinfo(result);
        return "127.0.0.1";
#else
        struct ifaddrs* ifaddr = nullptr;
        if (getifaddrs(&ifaddr) == -1) {
            return "127.0.0.1";
        }

        std::string result = "127.0.0.1";
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;
            if (ifa->ifa_addr->sa_family != AF_INET) continue;

            struct sockaddr_in* sockaddr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sockaddr->sin_addr, ipStr, INET_ADDRSTRLEN);

            // Skip loopback addresses
            if (std::string(ipStr) != "127.0.0.1") {
                result = ipStr;
                break;
            }
        }

        freeifaddrs(ifaddr);
        return result;
#endif
    }

    bool StringToAddress(const std::string& ip, struct sockaddr_in& addr) {
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        return inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) == 1;
    }

    std::string AddressToString(const struct sockaddr_in& addr) {
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ipStr, INET_ADDRSTRLEN);
        return std::string(ipStr);
    }

    std::string GetPublicIP() {
        // Use api.ipify.org which returns just the IP address as plain text
        const char* host = "api.ipify.org";
        const char* path = "/";
        const int port = 80;
        const int timeout_seconds = 5;

        // Initialize networking if not already done
        if (!g_initialized) {
            if (!Initialize()) {
                return "";
            }
        }

        // Create TCP socket
        SocketHandle sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET_HANDLE) {
            return "";
        }

        // Set socket to blocking for this operation
#ifdef _WIN32
        u_long mode = 0;
        ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
#endif

        // Set receive timeout
#ifdef _WIN32
        unsigned long timeout = timeout_seconds * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
        struct timeval timeout;
        timeout.tv_sec = timeout_seconds;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif

        // Resolve hostname
        struct addrinfo hints, *result = nullptr;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(host, "80", &hints, &result) != 0) {
            CloseSocket(sock);
            return "";
        }

        // Connect to server
        bool connected = false;
        for (struct addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
            if (connect(sock, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
                connected = true;
                break;
            }
        }

        freeaddrinfo(result);

        if (!connected) {
            CloseSocket(sock);
            return "";
        }

        // Send HTTP GET request
        std::string request = "GET " + std::string(path) + " HTTP/1.1\r\n";
        request += "Host: " + std::string(host) + "\r\n";
        request += "Connection: close\r\n";
        request += "User-Agent: GameServerManager/1.0\r\n";
        request += "\r\n";

        int sent = send(sock, request.c_str(), static_cast<int>(request.length()), 0);
        if (sent == SOCKET_ERROR_CODE || sent != static_cast<int>(request.length())) {
            CloseSocket(sock);
            return "";
        }

        // Receive response
        char buffer[1024];
        std::string response;
        int totalReceived = 0;
        
        while (totalReceived < static_cast<int>(sizeof(buffer) - 1)) {
            int received = recv(sock, buffer + totalReceived, 
                              static_cast<int>(sizeof(buffer) - 1 - totalReceived), 0);
            if (received <= 0) {
                break;
            }
            totalReceived += received;
        }
        buffer[totalReceived] = '\0';
        response = std::string(buffer);

        CloseSocket(sock);

        // Parse IP from response
        // Response format: "HTTP/1.1 200 OK\r\n...\r\n\r\n<IP>\r\n"
        // Or just: "<IP>\r\n" for some services
        size_t bodyStart = response.find("\r\n\r\n");
        if (bodyStart != std::string::npos) {
            bodyStart += 4; // Skip "\r\n\r\n"
        } else {
            bodyStart = 0;
        }

        // Extract IP address (first line after headers, remove whitespace)
        std::string ipLine = response.substr(bodyStart);
        size_t firstChar = ipLine.find_first_not_of(" \t\r\n");
        if (firstChar != std::string::npos) {
            size_t lastChar = ipLine.find_first_of(" \t\r\n", firstChar);
            if (lastChar != std::string::npos) {
                ipLine = ipLine.substr(firstChar, lastChar - firstChar);
            } else {
                ipLine = ipLine.substr(firstChar);
            }
        }

        // Validate it looks like an IP address (basic check)
        if (ipLine.find_first_not_of("0123456789.") == std::string::npos && 
            ipLine.length() > 0 && ipLine.length() < 16) {
            return ipLine;
        }

        return "";
    }
}

