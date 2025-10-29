#include "src/Object.h"
#include "src/ObjectSerializer.h"
#include "src/components/BodyComponent.h"
#include "src/components/SpriteComponent.h"
#include "src/components/InputComponent.h"
#include "src/components/PlayerMovementComponent.h"
#include <iostream>

int main() {
    // Load the level
    std::vector<std::unique_ptr<Object>> objects;
    if (!ObjectSerializer::loadObjectsFromFile(objects, "assets/level1.json")) {
        std::cerr << "Failed to load level!" << std::endl;
        return 1;
    }
    
    std::cout << "Loaded " << objects.size() << " objects" << std::endl;
    
    // Check each object
    for (size_t i = 0; i < objects.size(); ++i) {
        Object* obj = objects[i].get();
        
        auto* body = obj->getComponent<BodyComponent>();
        auto* sprite = obj->getComponent<SpriteComponent>();
        auto* input = obj->getComponent<InputComponent>();
        auto* movement = obj->getComponent<PlayerMovementComponent>();
        
        std::cout << "\nObject " << i << ":" << std::endl;
        
        if (body) {
            auto [x, y, angle] = body->getPosition();
            std::cout << "  ✓ BodyComponent: pos(" << x << ", " << y << ", " << angle << ")" << std::endl;
        } else {
            std::cout << "  ✗ Missing BodyComponent" << std::endl;
        }
        
        if (sprite) {
            std::cout << "  ✓ SpriteComponent" << std::endl;
        } else {
            std::cout << "  ✗ Missing SpriteComponent" << std::endl;
        }
        
        if (input) {
            std::cout << "  ✓ InputComponent (controllable)" << std::endl;
        }
        
        if (movement) {
            std::cout << "  ✓ PlayerMovementComponent (speed: " << movement->getMoveSpeed() << ")" << std::endl;
        }
    }
    
    return 0;
}

