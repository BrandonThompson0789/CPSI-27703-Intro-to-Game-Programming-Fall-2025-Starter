#ifndef ENGINE_H
#define ENGINE_H

#include <SDL.h>
#include <SDL_image.h>
#include <box2d/box2d.h>
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
        
        // Physics world access
        b2WorldId getPhysicsWorld() { return physicsWorldId; }
        
        static int screenWidth;
        static int screenHeight;
        static int targetFPS;
        
        // Conversion constants
        static constexpr float PIXELS_TO_METERS = 0.01f;
        static constexpr float METERS_TO_PIXELS = 100.0f;
        static constexpr float DEG_TO_RAD = 0.01745329251994329577f;
        static constexpr float RAD_TO_DEG = 57.295779513082320876f;

        static inline float degreesToRadians(float degrees) { return degrees * DEG_TO_RAD; }
        static inline float radiansToDegrees(float radians) { return radians * RAD_TO_DEG; }
        
    private:
        void processEvents();
        void update(float deltaTime);
        void render();
        static float getDeltaTime();
        SDL_Window* window;
        SDL_Renderer* renderer;
        bool running;
        std::vector<std::unique_ptr<Object>> objects;
        
        // Box2D physics world (v3.x uses handles/IDs instead of pointers)
        b2WorldId physicsWorldId;
};

#endif // ENGINE_H
