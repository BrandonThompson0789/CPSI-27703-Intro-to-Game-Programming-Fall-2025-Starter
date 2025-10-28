#include "Engine.h"
#include "components/BodyComponent.h"
#include "components/SpriteComponent.h"
#include "Object.h"
//#include <cstdio>
#include <iostream>


int main(int argc, char* argv[]) {
    //printf("Start of main\n");
    std::cout << "Start of main\n";
    Engine e;
    e.init();
    
    // Load the level from JSON file
    //e.loadFile("assets/level1.json");
    
    auto& objects = e.getObjects();
    objects.push_back(std::make_unique<Object>());
    Object& player = *objects[0];
    
    player.addComponent<BodyComponent>();
    player.addComponent<SpriteComponent>("player", true, true);
    player.getComponent<BodyComponent>()->setPosition(400.0f, 300.0f, 0.0f);


    e.run();
    e.cleanup();
    return 0;
}
