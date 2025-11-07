#include "Engine.h"
#include "SpriteManager.h"
#include "InputManager.h"
#include "CollisionManager.h"
#include "components/Component.h"
#include <fstream>
#include <iostream>
#include <cstdio>
#include <algorithm>

int Engine::screenWidth = 800;
int Engine::screenHeight = 600;
int Engine::targetFPS = 60;

void Engine::init() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        running = false;
        return;
    }
    
    // Create window
    window = SDL_CreateWindow("Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screenWidth, screenHeight, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        running = false;
        return;
    }
    
    // Create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        running = false;
        return;
    }
    
    debugDraw.init(renderer, METERS_TO_PIXELS);

    // Initialize Box2D physics world (v3.x API)
    // Gravity: (0, 0) for top-down game, use (0, 9.8) for side-scrollers
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = {0.0f, 0.0f};
    physicsWorldId = b2CreateWorld(&worldDef);
    if (collisionManager) {
        collisionManager->setWorld(physicsWorldId);
    }
    
    // Initialize sprite manager
    SpriteManager::getInstance().init(renderer, "assets/spriteData.json");
    
    // Initialize input manager
    InputManager::getInstance().init();
    
    std::cout << "SDL and Box2D initialized successfully!" << std::endl;
}

void Engine::run() {
    printf("Engine running\n");

    float deltaTime = getDeltaTime();
    Uint32 frameTime = 1000 / targetFPS;

    while (running) {
        Uint32 lastTime = SDL_GetTicks();
        
        processEvents();
        update(deltaTime);
        
        // Clear screen with black background
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        render();
        
        SDL_RenderPresent(renderer);

        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastTime < frameTime) {
            SDL_Delay(frameTime - (currentTime - lastTime));
        }
    }
}

void Engine::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_KEYDOWN:
                if (event.key.repeat == 0 && event.key.keysym.scancode == SDL_SCANCODE_F1) {
                    debugDraw.toggle();
                    std::cout << "Box2D debug draw " << (debugDraw.isEnabled() ? "enabled" : "disabled") << std::endl;
                }
                break;
                
            case SDL_CONTROLLERDEVICEADDED:
                // Handle hot-plug: controller connected
                InputManager::getInstance().handleControllerAdded(event.cdevice.which);
                break;
                
            case SDL_CONTROLLERDEVICEREMOVED:
                // Handle hot-plug: controller disconnected
                InputManager::getInstance().handleControllerRemoved(event.cdevice.which);
                break;
                
            default:
                break;
        }
    }
    
    // Check for escape key to quit
    const Uint8* state = SDL_GetKeyboardState(nullptr);
    if (state[SDL_SCANCODE_ESCAPE]) {
        running = false;
    }
    
    // Update input manager to poll all input sources
    InputManager::getInstance().update();
}

void Engine::update(float deltaTime) {
    // Step the Box2D physics simulation (v3.x API)
    // subStepCount controls accuracy (4 is default, higher = more accurate but slower)
    if (B2_IS_NON_NULL(physicsWorldId)) {
        b2World_Step(physicsWorldId, deltaTime, 4);
        if (collisionManager) {
            collisionManager->gatherCollisions();
            collisionManager->processCollisions(deltaTime);
        }
    }
    
    // Update all game objects
    for (auto& object : objects) {
        if (!object->isMarkedForDeath()) {
            object->update(deltaTime);
        }
    }

    // Remove objects that have been marked for death
    objects.erase(
        std::remove_if(
            objects.begin(),
            objects.end(),
            [](const std::unique_ptr<Object>& object) {
                return object->isMarkedForDeath();
            }),
        objects.end());

    // Add any queued objects after removals
    if (!pendingObjects.empty()) {
        for (auto& pending : pendingObjects) {
            objects.push_back(std::move(pending));
        }
        pendingObjects.clear();
    }
}

float Engine::getDeltaTime() {
    return 1.0f / targetFPS;
}

void Engine::render() {
    for (auto& object : objects) {
        if (!object->isMarkedForDeath()) {
            object->render(renderer);
        }
    }

    if (debugDraw.isEnabled() && B2_IS_NON_NULL(physicsWorldId)) {
        b2World_Draw(physicsWorldId, debugDraw.getInterface());
    }
}

void Engine::cleanup() {
    // Clean up objects before destroying physics world
    objects.clear();
    
    // Destroy physics world (v3.x API)
    if (B2_IS_NON_NULL(physicsWorldId)) {
        b2DestroyWorld(physicsWorldId);
        physicsWorldId = b2_nullWorldId;
        if (collisionManager) {
            collisionManager->setWorld(b2_nullWorldId);
            collisionManager->clearImpacts();
        }
    }
    
    InputManager::getInstance().cleanup();
    SpriteManager::getInstance().cleanup();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

Engine::Engine() {
    running = true;
    physicsWorldId = b2_nullWorldId;
    collisionManager = std::make_unique<CollisionManager>(*this);
}

Engine::~Engine() {
    cleanup();
}

void Engine::loadFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }

    nlohmann::json j;
    try {
        file >> j;
        file.close();
        std::cout << "JSON file parsed successfully" << std::endl;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        file.close();
        return;
    }

    // Clear existing objects
    objects.clear();
    pendingObjects.clear();
    if (collisionManager) {
        collisionManager->clearImpacts();
    }

    // Set Engine instance so objects can access it
    Object::setEngine(this);

    // Load objects from JSON using the component library
    if (!j.contains("objects") || !j["objects"].is_array()) {
        std::cerr << "Error: JSON file must contain an 'objects' array" << std::endl;
        return;
    }

    for (const auto& objectData : j["objects"]) {
        auto object = std::make_unique<Object>();
        object->fromJson(objectData);
        objects.push_back(std::move(object));
    }

    std::cout << "Loaded " << objects.size() << " objects from " << filename << std::endl;
}

void Engine::queueObject(std::unique_ptr<Object> object) {
    if (object) {
        pendingObjects.push_back(std::move(object));
    }
}