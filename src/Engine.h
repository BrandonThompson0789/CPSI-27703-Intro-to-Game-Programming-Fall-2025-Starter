#ifndef ENGINE_H
#define ENGINE_H

#include <SDL.h>
#include <SDL_image.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <nlohmann/json.hpp>
#include "Object.h"

class Engine {
    public:
        Engine();
        ~Engine();
        void init();
        void run();
        void cleanup();
        void loadFile(const std::string& filename);
        std::vector<std::unique_ptr<Object>>& getObjects() { return objects; }
        
        static int screenWidth;
        static int screenHeight;
        static int targetFPS;
    private:
        void processEvents();
        void update(float deltaTime);
        void render();
        static float getDeltaTime();
        SDL_Window* window;
        SDL_Renderer* renderer;
        bool running;
        std::vector<std::unique_ptr<Object>> objects;
};

#endif // ENGINE_H
