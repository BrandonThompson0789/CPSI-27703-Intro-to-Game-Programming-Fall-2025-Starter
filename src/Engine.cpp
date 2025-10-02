#include "Engine.h"
#include "Player.h"
#include "Enemy.h"
#include "Item.h"
#include "Library.h"
#include <fstream>
#include <iostream>
#include <cstdio>

std::unordered_map<std::string, bool> Engine::keyStates;
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
        if (event.type == SDL_QUIT) {
            running = false;
            break;
        }
    }
    SDL_PumpEvents();
    const Uint8* state = SDL_GetKeyboardState(NULL);
    keyStates.clear();
    
    if (state[SDL_SCANCODE_ESCAPE])
        running = false;
    
    if (state[SDL_SCANCODE_A] || state[SDL_SCANCODE_LEFT])
        keyStates["left"] = true;
    
    if (state[SDL_SCANCODE_D] || state[SDL_SCANCODE_RIGHT])
        keyStates["right"] = true;
    
    if (state[SDL_SCANCODE_W] || state[SDL_SCANCODE_UP])
        keyStates["up"] = true;
    
    if (state[SDL_SCANCODE_S] || state[SDL_SCANCODE_DOWN])
        keyStates["down"] = true;
    
    if (state[SDL_SCANCODE_SPACE])
        keyStates["jump"] = true;
    
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
    if (j.contains("player")) {
        auto it = Library::getLibrary().map.find("player");
        if (it != Library::getLibrary().map.end()) {
            auto player = it->second(j["player"]);
            objects.push_back(std::move(player));
        } else {
            std::cerr << "Error: Player factory not found in library" << std::endl;
        }
    }

    // Load enemies using Library
    if (j.contains("enemies")) {
        auto it = Library::getLibrary().map.find("enemy");
        if (it != Library::getLibrary().map.end()) {
            for (const auto& enemyData : j["enemies"]) {
                auto enemy = it->second(enemyData);
                objects.push_back(std::move(enemy));
            }
        } else {
            std::cerr << "Error: Enemy factory not found in library" << std::endl;
        }
    }

    // Load items using Library
    if (j.contains("items")) {
        auto it = Library::getLibrary().map.find("item");
        if (it != Library::getLibrary().map.end()) {
            for (const auto& itemData : j["items"]) {
                auto item = it->second(itemData);
                objects.push_back(std::move(item));
            }
        } else {
            std::cerr << "Error: Item factory not found in library" << std::endl;
        }
    }

    std::cout << "Loaded " << objects.size() << " objects from " << filename << std::endl;
    std::cout << "Objects loaded: " << objects.size() << " (1 player, " << (objects.size() > 1 ? objects.size() - 1 : 0) << " others)" << std::endl;
}