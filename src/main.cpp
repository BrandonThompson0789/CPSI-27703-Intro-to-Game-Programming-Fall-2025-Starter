#include "Engine.h"
#include "InputManager.h"
#include "components/BodyComponent.h"
#include "components/InputComponent.h"
#include "components/PlayerMovementComponent.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "=== Engine Start ===" << std::endl;
    std::cout << "Controls: WASD/Arrows to move, Shift to walk, Space/E to interact" << std::endl;
    
    Engine e;
    e.init();
    
    // Check for connected controllers
    InputManager& inputMgr = InputManager::getInstance();
    int numControllers = inputMgr.getNumControllers();
    std::cout << "Connected controllers: " << numControllers << std::endl;
    
    // Load the level from JSON file
    e.loadFile("assets/level1.json");
    
    std::cout << "Level loaded! Center object is controllable with keyboard/controller." << std::endl;
    std::cout << "Press ESC to quit" << std::endl;

    e.run();
    e.cleanup();
    
    std::cout << "Engine finished" << std::endl;
    return 0;
}
