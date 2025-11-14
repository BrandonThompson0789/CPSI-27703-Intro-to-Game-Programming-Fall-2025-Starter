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

int main(int argc, char* argv[]) {
    uint16_t port = 8888;
    
    // Parse command line arguments
    if (argc > 1) {
        try {
            port = static_cast<uint16_t>(std::stoi(argv[1]));
        } catch (...) {
            std::cerr << "Invalid port number. Using default: 8888" << std::endl;
        }
    }

    std::cout << "=== Server Manager ===" << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;

    ServerManager serverManager(port);
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

