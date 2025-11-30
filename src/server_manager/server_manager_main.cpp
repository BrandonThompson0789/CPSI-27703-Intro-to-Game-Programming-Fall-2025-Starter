#include "ServerManager.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

static std::atomic<bool> g_running(true);
static ServerManager* g_serverManager = nullptr;

void SignalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutting down server manager..." << std::endl;
        g_running = false;
        if (g_serverManager) {
            g_serverManager->Shutdown();
        }
    }
}

void PrintUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --port <port>              Port to listen on (default: 8888)" << std::endl;
    std::cout << "  --no-relay                 Disable relay connections" << std::endl;
    std::cout << "  --force-direct             Force direct connections only" << std::endl;
    std::cout << "  --force-nat                Force NAT punchthrough only" << std::endl;
    std::cout << "  --force-relay              Force relay connections only" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " --port 8888 --no-relay" << std::endl;
    std::cout << "  " << programName << " --force-direct" << std::endl;
    std::cout << "  " << programName << " --force-nat" << std::endl;
    std::cout << "  " << programName << " --force-relay" << std::endl;
}

int main(int argc, char* argv[]) {
    uint16_t port = 8888;
    bool relayEnabled = true;
    ForcedConnectionType forcedType = ForcedConnectionType::NONE;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg == "--port" && i + 1 < argc) {
            try {
                port = static_cast<uint16_t>(std::stoi(argv[++i]));
            } catch (...) {
                std::cerr << "Invalid port number: " << argv[i] << std::endl;
                return 1;
            }
        } else if (arg == "--no-relay") {
            relayEnabled = false;
        } else if (arg == "--force-direct") {
            forcedType = ForcedConnectionType::DIRECT_ONLY;
        } else if (arg == "--force-nat") {
            forcedType = ForcedConnectionType::NAT_ONLY;
        } else if (arg == "--force-relay") {
            forcedType = ForcedConnectionType::RELAY_ONLY;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            PrintUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "=== Server Manager ===" << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "Relay enabled: " << (relayEnabled ? "yes" : "no") << std::endl;
    
    std::string forcedTypeStr;
    switch (forcedType) {
        case ForcedConnectionType::NONE:
            forcedTypeStr = "none (normal fallback)";
            break;
        case ForcedConnectionType::DIRECT_ONLY:
            forcedTypeStr = "direct only";
            break;
        case ForcedConnectionType::NAT_ONLY:
            forcedTypeStr = "NAT punchthrough only";
            break;
        case ForcedConnectionType::RELAY_ONLY:
            forcedTypeStr = "relay only";
            break;
    }
    std::cout << "Forced connection type: " << forcedTypeStr << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;

    ServerManager serverManager(port);
    serverManager.SetRelayEnabled(relayEnabled);
    serverManager.SetForcedConnectionType(forcedType);
    g_serverManager = &serverManager;

    // Set up signal handlers
#ifdef _WIN32
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
#else
    struct sigaction sa;
    sa.sa_handler = SignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
#endif

    if (!serverManager.Initialize()) {
        std::cerr << "Failed to initialize server manager" << std::endl;
        return 1;
    }

    // Run in a separate thread so we can check g_running
    std::thread serverThread([&serverManager]() {
        serverManager.Run();
    });

    // Wait for shutdown signal
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    serverThread.join();

    std::cout << "Server Manager exited." << std::endl;
    return 0;
}

