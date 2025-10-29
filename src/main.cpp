#include "Engine.h"
#include "components/BodyComponent.h"
#include "components/SpriteComponent.h"
#include "components/InputComponent.h"
#include "components/PlayerMovementComponent.h"
#include "InputManager.h"
#include "Object.h"
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
    //e.loadFile("assets/level1.json");
    
    auto& objects = e.getObjects();
    objects.push_back(std::make_unique<Object>());
    Object& player = *objects[0];
    
    // Add components with input support
    // Player can use BOTH keyboard and controller 0 - whichever provides input wins!
    player.addComponent<BodyComponent>();
    player.addComponent<SpriteComponent>("player", true, true);
    player.addComponent<InputComponent>(std::vector<int>{INPUT_SOURCE_KEYBOARD, 0});  // Keyboard + Controller 0
    player.addComponent<PlayerMovementComponent>(200.0f);        // Movement with 200 px/sec speed
    player.getComponent<BodyComponent>()->setPosition(400.0f, 300.0f, 0.0f);

    std::cout << "Player created with multi-source input support!" << std::endl;
    std::cout << "You can use BOTH keyboard and controller!" << std::endl;
    std::cout << "Press ESC to quit" << std::endl;

    e.run();
    e.cleanup();
    
    std::cout << "Engine finished" << std::endl;
    return 0;
}
