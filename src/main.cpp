#include "Engine.h"
#include "InputManager.h"
#include "components/BodyComponent.h"
#include "components/InputComponent.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::cout << "=== Engine Start ===" << std::endl;
    //std::cout << "Controls: WASD/Arrows to move, Shift to walk, Space/E to interact" << std::endl;
    
    // Parse command-line arguments
    bool hostMode = false;
    uint16_t hostPort = 8889;
    std::string serverManagerIP = "127.0.0.1";
    uint16_t serverManagerPort = 8888;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--host") {
            hostMode = true;
        } else if (arg == "--host-port" && i + 1 < argc) {
            hostPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--server-manager-ip" && i + 1 < argc) {
            serverManagerIP = argv[++i];
        } else if (arg == "--server-manager-port" && i + 1 < argc) {
            serverManagerPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --host                    Start in host mode (multiplayer)" << std::endl;
            std::cout << "  --host-port PORT          Host port (default: 8889)" << std::endl;
            std::cout << "  --server-manager-ip IP    Server Manager IP (default: 127.0.0.1)" << std::endl;
            std::cout << "  --server-manager-port PORT Server Manager port (default: 8888)" << std::endl;
            std::cout << "  --help, -h                Show this help message" << std::endl;
            return 0;
        }
    }
    
    Engine e;
    e.init();
    
    // Check for connected controllers
    InputManager& inputMgr = InputManager::getInstance();
    int numControllers = inputMgr.getNumControllers();
    std::cout << "Connected controllers: " << numControllers << std::endl;
    
    // Load the level from JSON file
    e.loadFile("assets/level1.json");
    InputManager::getInstance().loadNamedConfig("arrows", "assets/input_config_arrows.json");
    InputManager::getInstance().loadNamedConfig("default", "assets/input_config.json");
    
    std::cout << "Level loaded! Center object is controllable with keyboard/controller." << std::endl;
    
    // Start hosting if requested
    if (hostMode) {
        std::cout << "\n=== Starting Host Mode ===" << std::endl;
        std::cout << "Connecting to Server Manager at " << serverManagerIP << ":" << serverManagerPort << "..." << std::endl;
        
        std::string roomCode = e.startHosting(hostPort, serverManagerIP, serverManagerPort);
        
        if (!roomCode.empty()) {
            std::cout << "\n*** HOST MODE ACTIVE ***" << std::endl;
            std::cout << "Room Code: " << roomCode << std::endl;
            std::cout << "Share this room code with clients to join your game!" << std::endl;
            std::cout << "Hosting on port: " << hostPort << std::endl;
            std::cout << "Waiting for clients to connect..." << std::endl;
        } else {
            std::cerr << "ERROR: Failed to start hosting!" << std::endl;
            std::cerr << "Make sure the Server Manager is running." << std::endl;
            e.cleanup();
            return 1;
        }
    }
    
    std::cout << "Press ESC to quit" << std::endl;

    e.run();
    e.cleanup();
    
    std::cout << "Engine finished" << std::endl;
    return 0;
}
