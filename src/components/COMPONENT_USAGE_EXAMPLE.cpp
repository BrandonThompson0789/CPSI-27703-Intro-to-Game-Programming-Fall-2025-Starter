// EXAMPLE: How to use the component system with Object class
// This is a reference file - not compiled into the project

#include "Object.h"
#include "components/BodyComponent.h"
#include "components/SpriteComponent.h"

// ==============================================================
// EXAMPLE 1: Creating an object with components in Engine::run()
// ==============================================================

void exampleInEngine() {
    // Create a new object
    std::unique_ptr<Object> player = std::make_unique<Object>();
    
    // Add components
    player->addComponent<BodyComponent>();
    player->addComponent<SpriteComponent>("player");
    
    // Set initial position
    player->setPosition(400.0f, 300.0f, 0.0f);
    
    // The object will automatically call update() and draw() on all components
}

// ==============================================================
// EXAMPLE 2: Custom object class with components
// ==============================================================

class CustomPlayer : public Object {
private:
    BodyComponent* body;
    SpriteComponent* sprite;
    float speed = 5.0f;

public:
    CustomPlayer() {
        // Add components and keep pointers
        body = addComponent<BodyComponent>();
        sprite = addComponent<SpriteComponent>("player");
        
        // Setup initial state
        setPosition(400, 300, 0);
        sprite->setAnimationSpeed(10.0f);
        sprite->playAnimation(true);
    }
    
    void update() override {
        // Custom update logic before components
        if (/* moving right */) {
            sprite->setFlipHorizontal(false);
        } else if (/* moving left */) {
            sprite->setFlipHorizontal(true);
        }
        
        // Call base Object::update() to update all components
        Object::update();
        
        // Custom update logic after components
    }
    
    void render(SDL_Renderer* renderer) override {
        // Call base Object::render() to draw all components
        Object::render(renderer);
        
        // Optional: Add custom rendering after components
    }
};

// ==============================================================
// EXAMPLE 3: Accessing components later
// ==============================================================

void accessComponentsExample(Object* obj) {
    // Check if object has a component
    if (obj->hasComponent<SpriteComponent>()) {
        // Get the component
        SpriteComponent* sprite = obj->getComponent<SpriteComponent>();
        
        // Use it
        sprite->setAlpha(128);
        sprite->playAnimation(true);
    }
    
    // Get body component
    BodyComponent* body = obj->getComponent<BodyComponent>();
    if (body) {
        body->setVelocity(10.0f, 0.0f, 0.0f);
    }
}

// ==============================================================
// EXAMPLE 4: Complete example in Engine::run()
// ==============================================================

/*
void Engine::run() {
    printf("Engine running\n");
    
    // Create player object
    objects.push_back(std::make_unique<Object>());
    Object& player = *objects[0];
    
    // Add components
    player.addComponent<BodyComponent>();
    player.addComponent<SpriteComponent>("player");
    player.setPosition(400.0f, 300.0f, 0.0f);
    
    // Get sprite component to configure it
    SpriteComponent* sprite = player.getComponent<SpriteComponent>();
    if (sprite) {
        sprite->setAnimationSpeed(8.0f);
        sprite->playAnimation(true);
    }

    while (running) {
        Uint32 lastTime = SDL_GetTicks();
        
        processEvents();
        update();  // Calls player.update() which updates all components
        
        // Clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        render();  // Calls player.render() which draws all components
        
        SDL_RenderPresent(renderer);

        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastTime < 1000/60) {
            SDL_Delay(1000/60 - (currentTime - lastTime));
        }
    }
}
*/

// ==============================================================
// EXAMPLE 5: Building complex objects with multiple components
// ==============================================================

class Enemy : public Object {
public:
    Enemy(float x, float y) {
        // Add components
        auto body = addComponent<BodyComponent>();
        auto sprite = addComponent<SpriteComponent>("enemy");
        
        // Configure
        body->setPosition(x, y, 0.0f);
        sprite->setAnimationSpeed(5.0f);
        sprite->playAnimation(true);
    }
    
    void update() override {
        // AI logic here
        
        // Update components
        Object::update();
    }
};

