#include "Engine.h"
#include "SpriteManager.h"
#include "InputManager.h"
#include <fstream>
#include <iostream>
#include <cstdio>

int Engine::screenWidth = 800;
int Engine::screenHeight = 600;

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
    
    // Initialize sprite manager
    SpriteManager::getInstance().init(renderer, "assets/spriteData.json");
    
    // Initialize input manager
    InputManager::getInstance().init();
    
    std::cout << "SDL initialized successfully!" << std::endl;
}

void Engine::run() {
    printf("Engine running\n");

    while (running) {
        Uint32 lastTime = SDL_GetTicks();
        
        processEvents();
        update();
        
        // Clear screen with black background
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        render();
        
        SDL_RenderPresent(renderer);

        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastTime < 1000/60) {
            SDL_Delay(1000/60 - (currentTime - lastTime));
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
                
            case SDL_CONTROLLERDEVICEADDED:
                std::cout << "Controller connected: " << event.cdevice.which << std::endl;
                // InputManager will handle this on next update
                break;
                
            case SDL_CONTROLLERDEVICEREMOVED:
                std::cout << "Controller disconnected: " << event.cdevice.which << std::endl;
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

void Engine::update() {
    for (auto& object : objects) {
        object->update();
    }
}

void Engine::render() {
    for (auto& object : objects) {
        object->render(renderer);
    }
}

void Engine::cleanup() {
    InputManager::getInstance().cleanup();
    SpriteManager::getInstance().cleanup();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

Engine::Engine() {
    running = true;
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

    // Load player using Library
   

    std::cout << "Loaded " << objects.size() << " objects from " << filename << std::endl;
    std::cout << "Objects loaded: " << objects.size() << " (1 player, " << (objects.size() > 1 ? objects.size() - 1 : 0) << " others)" << std::endl;
}