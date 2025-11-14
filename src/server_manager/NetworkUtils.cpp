#include "NetworkUtils.h"
#include <cstring>
#include <iostream>

#ifndef _WIN32
    #include <fcntl.h>
    #include <ifaddrs.h>
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
}

