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
#include "Box2DDebugDraw.h"

class CollisionManager;
class BackgroundManager;
class HostManager;
class ClientManager;
class MenuManager;

class Engine {
    public:
        Engine();
        ~Engine();
        void init();
        void run();
        void cleanup();
        void loadFile(const std::string& filename);
        std::vector<std::unique_ptr<Object>>& getObjects() { return objects; }
        void queueObject(std::unique_ptr<Object> object);
        std::vector<std::unique_ptr<Object>>& getQueuedObjects() { return pendingObjects; }
        CollisionManager* getCollisionManager() { return collisionManager.get(); }
        BackgroundManager* getBackgroundManager() { return backgroundManager.get(); }
        HostManager* getHostManager() { return hostManager.get(); }
        ClientManager* getClientManager() { return clientManager.get(); }
        MenuManager* getMenuManager() { return menuManager.get(); }
        
        // Start hosting (returns room code on success, empty string on failure)
        std::string startHosting(uint16_t hostPort = 8889, const std::string& serverManagerIP = "127.0.0.1", uint16_t serverManagerPort = 8888);
        void stopHosting();
        bool isHosting() const;
        
        // Connect as client (returns true on success, false on failure)
        bool connectAsClient(const std::string& roomCode, const std::string& serverManagerIP = "127.0.0.1", uint16_t serverManagerPort = 8888);
        void disconnectClient();
        bool isClient() const;
        
        // Physics world access
        b2WorldId getPhysicsWorld() { return physicsWorldId; }
        
        // Renderer access (for menu system)
        SDL_Renderer* getRenderer() const { return renderer; }
        
        // Quit the engine (sets running to false)
        void quit() { running = false; }
        
        static int screenWidth;
        static int screenHeight;
        static int targetFPS;
        static float deltaTime;
        
        // Conversion constants
        static constexpr float PIXELS_TO_METERS = 0.01f;
        static constexpr float METERS_TO_PIXELS = 100.0f;
        static constexpr float DEG_TO_RAD = 0.01745329251994329577f;
        static constexpr float RAD_TO_DEG = 57.295779513082320876f;

        static inline float degreesToRadians(float degrees) { return degrees * DEG_TO_RAD; }
        static inline float radiansToDegrees(float radians) { return radians * RAD_TO_DEG; }

        struct CameraState {
            float viewMinX = -800.0f;
            float viewMinY = -450.0f;
            float viewWidth = 1600.0f;
            float viewHeight = 900.0f;
            float scale = 1.0f;
        };

        const CameraState& getCameraState() const { return cameraState; }
        SDL_FPoint worldToScreen(float worldX, float worldY) const;
        float worldToScreenLength(float value) const;
        float getCameraScale() const { return cameraState.scale; }
        void applyViewBounds(float minX, float minY, float maxX, float maxY);
        void ensureDefaultCamera();
        
        // Template support for object spawning
        nlohmann::json buildObjectDefinition(const nlohmann::json& objectData) const;
        
    private:
        void loadObjectTemplates(const std::string& filename);
        static void mergeJsonObjects(nlohmann::json& target, const nlohmann::json& overrides);
        static void mergeComponentData(nlohmann::json& baseComponent, const nlohmann::json& overrideComponent);
        static nlohmann::json mergeObjectDefinitions(const nlohmann::json& baseObject, const nlohmann::json& overrides);
        void processEvents();
        void update(float deltaTime);
        void render();
        void onWindowResized(int width, int height);
        static float getDeltaTime();
        SDL_Window* window;
        SDL_Renderer* renderer;
        bool running;
        bool cleanedUp;
        std::vector<std::unique_ptr<Object>> objects;
        std::vector<std::unique_ptr<Object>> pendingObjects;
        std::unique_ptr<CollisionManager> collisionManager;
        
        // Box2D physics world (v3.x uses handles/IDs instead of pointers)
        b2WorldId physicsWorldId;
        Box2DDebugDraw debugDraw;
        std::unordered_map<std::string, nlohmann::json> objectTemplates;
        std::unique_ptr<BackgroundManager> backgroundManager;
        std::unique_ptr<HostManager> hostManager;
        std::unique_ptr<ClientManager> clientManager;
        std::unique_ptr<MenuManager> menuManager;

        CameraState cameraState;
        CameraState cameraTarget;
        bool cameraInitialized = false;
        static constexpr float MIN_CAMERA_WIDTH = 800.0f;
        static constexpr float MIN_CAMERA_HEIGHT = 450.0f;
        static constexpr float CAMERA_SMOOTHING_RATE = 8.0f;
        float lastDeltaTime = 1.0f / 60.0f;
};

#endif // ENGINE_H
