#include "Engine.h"
#include "SpriteManager.h"
#include "InputManager.h"
#include "SoundManager.h"
#include "SensorEventManager.h"
#include "CollisionManager.h"
#include "BackgroundManager.h"
#include "HostManager.h"
#include "ClientManager.h"
#include "menus/MenuManager.h"
#include "components/Component.h"
#include "components/ViewGrabComponent.h"
#include "PlayerManager.h"
#include "SaveManager.h"
#include <cmath>
#include <SDL_ttf.h>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <algorithm>


int Engine::screenWidth = 800;
int Engine::screenHeight = 600;
int Engine::targetFPS = 60;
float Engine::deltaTime = 1.0f / Engine::targetFPS;

void Engine::init() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        running = false;
        return;
    }
    
    // Create window
    window = SDL_CreateWindow("Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screenWidth, screenHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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

    if (TTF_Init() == -1) {
        std::cerr << "SDL_ttf could not initialize! TTF_Error: " << TTF_GetError() << std::endl;
    }
    
    debugDraw.init(renderer, METERS_TO_PIXELS);
    if (TTF_WasInit()) {
        if (!debugDraw.setLabelFont("assets/fonts/ARIAL.TTF", 14)) {
            std::cerr << "Warning: Failed to configure debug draw font." << std::endl;
        }
    }
    debugDraw.setCamera(cameraState.scale, cameraState.viewMinX, cameraState.viewMinY);

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

    // Initialize background manager
    backgroundManager = std::make_unique<BackgroundManager>();
    backgroundManager->init(renderer);

    // Initialize sound manager (non-fatal if missing)
    if (!SoundManager::getInstance().isInitialized()) {
        if (!SoundManager::getInstance().init("assets/soundData.json", this)) {
            std::cerr << "Warning: SoundManager failed to initialize" << std::endl;
        }
    } else {
        SoundManager::getInstance().init("assets/soundData.json", this);
    }

    // Initialize input manager
    InputManager::getInstance().init();
    
    // Initialize default player assignments (keyboard -> player 1, first controller -> player 1)
    PlayerManager::getInstance().initializeDefaultAssignments();
    
    // Initialize menu manager
    menuManager = std::make_unique<MenuManager>(this);
    menuManager->init();
    
    // Load overall save data (metadata, progress, settings) on game start
    // This will create a default save file if one doesn't exist
    SaveManager::getInstance().loadSaveData("save.json");
    
    std::cout << "SDL and Box2D initialized successfully!" << std::endl;
}

void Engine::run() {
    printf("Engine running\n");

    Uint32 frameTime = 1000 / targetFPS;
    Uint32 previousTicks = SDL_GetTicks();

    while (running) {
        Uint32 frameStartTicks = SDL_GetTicks();
        Uint32 elapsedSinceLastFrame = frameStartTicks - previousTicks;
        previousTicks = frameStartTicks;
        float frameDeltaSeconds = elapsedSinceLastFrame > 0
                                      ? static_cast<float>(elapsedSinceLastFrame) / 1000.0f
                                      : 1.0f / static_cast<float>(targetFPS);
        deltaTime = frameDeltaSeconds;
        
        processEvents();
        update(deltaTime);
        
        // Clear screen with black background
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        render();
        
        SDL_RenderPresent(renderer);

        Uint32 frameEndTicks = SDL_GetTicks();
        Uint32 frameDuration = frameEndTicks - frameStartTicks;
        if (frameDuration < frameTime) {
            SDL_Delay(frameTime - frameDuration);
        }
    }
}

void Engine::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Let menu manager handle events first if menu is active
        if (menuManager && menuManager->isMenuActive()) {
            menuManager->handleEvent(event);
        }
        
        switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_KEYDOWN:
                // Only handle F1 debug toggle if menu is not active
                if (!menuManager || !menuManager->isMenuActive()) {
                    if (event.key.repeat == 0 && event.key.keysym.scancode == SDL_SCANCODE_F1) {
                        debugDraw.toggle();
                        std::cout << "Box2D debug draw " << (debugDraw.isEnabled() ? "enabled" : "disabled") << std::endl;
                    }
                }
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    onWindowResized(event.window.data1, event.window.data2);
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
    
    // Escape key quit is now handled by pause action (no longer directly quit)
    
    // Update input manager to poll all input sources
    InputManager::getInstance().update();
}

void Engine::update(float deltaTime) {
    lastDeltaTime = deltaTime;

    // Update menu manager (always update, even if paused)
    if (menuManager) {
        menuManager->update(deltaTime);
    }
    
    // Update message display system (always update, even when paused)
    updateMessages(deltaTime);
    
    // Check if game should be paused (menu active)
    bool shouldPause = menuManager && menuManager->shouldPauseGame();
    
    if (shouldPause) {
        // Don't update game objects when menu is active (except network input if hosting)
        // Still update network managers even when paused
        if (hostManager && hostManager->IsHosting()) {
            hostManager->Update(deltaTime);
        }
        if (clientManager && clientManager->IsConnected()) {
            clientManager->Update(deltaTime);
        }
        return;
    }
    
    // Check for pause input when game is running (no menu active)
    InputManager& inputMgr = InputManager::getInstance();
    float pause = inputMgr.getInputValue(INPUT_SOURCE_KEYBOARD, GameAction::ACTION_PAUSE);
    for (int i = 0; i < 4; ++i) {
        if (inputMgr.isInputSourceActive(i)) {
            float p = inputMgr.getInputValue(i, GameAction::ACTION_PAUSE);
            if (p > pause) pause = p;
        }
    }
    
    static bool lastPauseState = false;
    bool currentPauseState = (pause > 0.5f);
    if (currentPauseState && !lastPauseState) {
        // Open pause menu
        if (menuManager) {
            menuManager->openMenu("pause");
        }
    }
    lastPauseState = currentPauseState;

    // Step the Box2D physics simulation (v3.x API)
    // subStepCount controls accuracy (4 is default, higher = more accurate but slower)
    if (B2_IS_NON_NULL(physicsWorldId)) {
        b2World_Step(physicsWorldId, deltaTime, 4);
        if (collisionManager) {
            collisionManager->gatherCollisions();
            collisionManager->processCollisions(deltaTime);
        }
        SensorEventManager::getInstance().processWorldEvents(physicsWorldId);
    }
    
    ViewGrabComponent::beginFrame();

    // Update all game objects
    for (auto& object : objects) {
        if (!object->isMarkedForDeath()) {
            object->update(deltaTime);
        }
    }

    ViewGrabComponent::finalizeFrame(*this);

    // Track objects being destroyed for HostManager
    std::vector<Object*> destroyedObjects;
    for (auto& object : objects) {
        if (object->isMarkedForDeath()) {
            destroyedObjects.push_back(object.get());
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
    std::vector<Object*> createdObjects;
    if (!pendingObjects.empty()) {
        for (auto& pending : pendingObjects) {
            createdObjects.push_back(pending.get());
            objects.push_back(std::move(pending));
        }
        pendingObjects.clear();
    }

    // Notify HostManager of object changes
    if (hostManager && hostManager->IsHosting()) {
        for (Object* obj : destroyedObjects) {
            hostManager->SendObjectDestroy(obj);
        }
        for (Object* obj : createdObjects) {
            hostManager->SendObjectCreate(obj);
        }
    }

    // Update HostManager (only when not paused - network updates handled in pause block)
    if (hostManager && hostManager->IsHosting() && !shouldPause) {
        hostManager->Update(deltaTime);
    }

    // Update ClientManager (only when not paused - network updates handled in pause block)
    if (clientManager && clientManager->IsConnected() && !shouldPause) {
        clientManager->Update(deltaTime);
    }
}

float Engine::getDeltaTime() {
    return deltaTime;
}

void Engine::render() {
    // Render background first (before all objects)
    if (backgroundManager) {
        backgroundManager->render(this);
    }
    
    // Render all game objects
    for (auto& object : objects) {
        if (!object->isMarkedForDeath()) {
            object->render(renderer);
        }
    }

    if (debugDraw.isEnabled() && B2_IS_NON_NULL(physicsWorldId)) {
        b2World_Draw(physicsWorldId, debugDraw.getInterface());
        debugDraw.renderLabels(objects);
    }
    
    // Render menus on top of everything
    if (menuManager) {
        menuManager->render();
    }
    
    // Render messages on top of everything (including menus)
    renderMessages();
}

void Engine::onWindowResized(int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);

    screenWidth = width;
    screenHeight = height;

    auto recomputeScale = [width, height](const CameraState& cam) {
        float viewWidth = std::max(cam.viewWidth, MIN_CAMERA_WIDTH);
        float viewHeight = std::max(cam.viewHeight, MIN_CAMERA_HEIGHT);
        float scaleX = static_cast<float>(width) / viewWidth;
        float scaleY = static_cast<float>(height) / viewHeight;
        float scale = std::min(scaleX, scaleY);
        return scale > 0.0f ? scale : 1.0f;
    };

    cameraTarget.scale = recomputeScale(cameraTarget);
    cameraState.scale = recomputeScale(cameraState);

    debugDraw.setCamera(cameraState.scale, cameraState.viewMinX, cameraState.viewMinY);
}

SDL_FPoint Engine::worldToScreen(float worldX, float worldY) const {
    return {
        (worldX - cameraState.viewMinX) * cameraState.scale,
        (worldY - cameraState.viewMinY) * cameraState.scale
    };
}

float Engine::worldToScreenLength(float value) const {
    return value * cameraState.scale;
}

void Engine::applyViewBounds(float minX, float minY, float maxX, float maxY) {
    if (minX > maxX) {
        std::swap(minX, maxX);
    }
    if (minY > maxY) {
        std::swap(minY, maxY);
    }

    float desiredWidth = std::max(maxX - minX, MIN_CAMERA_WIDTH);
    float desiredHeight = std::max(maxY - minY, MIN_CAMERA_HEIGHT);

    float scaleX = static_cast<float>(screenWidth) / desiredWidth;
    float scaleY = static_cast<float>(screenHeight) / desiredHeight;

    float desiredScale = std::min(scaleX, scaleY);
    if (desiredScale <= 0.0f) {
        desiredScale = 1.0f;
    }

    float viewWidth = static_cast<float>(screenWidth) / desiredScale;
    float viewHeight = static_cast<float>(screenHeight) / desiredScale;

    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;

    float viewMinX = centerX - viewWidth * 0.5f;
    float viewMinY = centerY - viewHeight * 0.5f;

    cameraTarget.viewMinX = viewMinX;
    cameraTarget.viewMinY = viewMinY;
    cameraTarget.viewWidth = viewWidth;
    cameraTarget.viewHeight = viewHeight;
    cameraTarget.scale = desiredScale;

    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    if (!cameraInitialized) {
        cameraState = cameraTarget;
        cameraInitialized = true;
    } else {
        float smoothingRate = std::max(CAMERA_SMOOTHING_RATE, 0.0f);
        float smoothingFactor = smoothingRate > 0.0f
                                    ? 1.0f - std::exp(-smoothingRate * std::max(lastDeltaTime, 0.0f))
                                    : 1.0f;
        smoothingFactor = std::clamp(smoothingFactor, 0.0f, 1.0f);

        cameraState.viewMinX = lerp(cameraState.viewMinX, cameraTarget.viewMinX, smoothingFactor);
        cameraState.viewMinY = lerp(cameraState.viewMinY, cameraTarget.viewMinY, smoothingFactor);
        cameraState.viewWidth = lerp(cameraState.viewWidth, cameraTarget.viewWidth, smoothingFactor);
        cameraState.viewHeight = lerp(cameraState.viewHeight, cameraTarget.viewHeight, smoothingFactor);
        cameraState.scale = lerp(cameraState.scale, cameraTarget.scale, smoothingFactor);
    }

    debugDraw.setCamera(cameraState.scale, cameraState.viewMinX, cameraState.viewMinY);
}

void Engine::ensureDefaultCamera() {
    float defaultScale = std::min(
        static_cast<float>(screenWidth) / MIN_CAMERA_WIDTH,
        static_cast<float>(screenHeight) / MIN_CAMERA_HEIGHT);
    if (defaultScale <= 0.0f) {
        defaultScale = 1.0f;
    }

    cameraTarget.viewWidth = MIN_CAMERA_WIDTH;
    cameraTarget.viewHeight = MIN_CAMERA_HEIGHT;
    cameraTarget.viewMinX = -0.5f * cameraTarget.viewWidth;
    cameraTarget.viewMinY = -0.5f * cameraTarget.viewHeight;
    cameraTarget.scale = defaultScale;

    if (!cameraInitialized) {
        cameraState = cameraTarget;
        cameraInitialized = true;
    } else {
        cameraState = cameraTarget;
    }

    debugDraw.setCamera(cameraState.scale, cameraState.viewMinX, cameraState.viewMinY);
}

void Engine::cleanup() {
    if (cleanedUp) {
        return;
    }
    cleanedUp = true;

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
        SensorEventManager::getInstance().clear();
    }
    InputManager::getInstance().cleanup();
    SpriteManager::getInstance().cleanup();
    SoundManager::getInstance().shutdown();
    if (backgroundManager) {
        backgroundManager->cleanup();
        backgroundManager.reset();
    }
    if (hostManager) {
        hostManager->Shutdown();
        hostManager.reset();
    }
    if (clientManager) {
        clientManager->Disconnect();
        clientManager.reset();
    }
    if (menuManager) {
        menuManager->cleanup();
        menuManager.reset();
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    debugDraw.shutdown();
    if (TTF_WasInit()) {
        TTF_Quit();
    }
    SDL_Quit();
}

Engine::Engine() {
    running = true;
    cleanedUp = false;
    window = nullptr;
    renderer = nullptr;
    physicsWorldId = b2_nullWorldId;
    collisionManager = std::make_unique<CollisionManager>(*this);
    backgroundManager = nullptr;
    cameraState.viewWidth = MIN_CAMERA_WIDTH;
    cameraState.viewHeight = MIN_CAMERA_HEIGHT;
    cameraState.viewMinX = -0.5f * cameraState.viewWidth;
    cameraState.viewMinY = -0.5f * cameraState.viewHeight;
    cameraState.scale = std::min(
        static_cast<float>(screenWidth) / cameraState.viewWidth,
        static_cast<float>(screenHeight) / cameraState.viewHeight);
    if (cameraState.scale <= 0.0f) {
        cameraState.scale = 1.0f;
    }
    cameraTarget = cameraState;
    cameraInitialized = false;
    lastDeltaTime = getDeltaTime();
    currentMessage = nullptr;
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

    // Check if this is a save file (has metadata)
    // If so, extract just the level data (background + objects)
    nlohmann::json levelData = j;
    bool isSaveFile = j.contains("metadata");
    if (isSaveFile) {
        // This is a save file, extract level data
        levelData = nlohmann::json::object();
        if (j.contains("background")) {
            levelData["background"] = j["background"];
        }
        if (j.contains("objects")) {
            levelData["objects"] = j["objects"];
        }
        
        // If save file has no level data, it's invalid for loading
        if (!levelData.contains("objects") || !levelData["objects"].is_array()) {
            std::cerr << "Error: Save file does not contain level data (objects array)" << std::endl;
            return;
        }
    }

    loadObjectTemplates("assets/objectData.json");

    // Clear existing objects
    objects.clear();
    pendingObjects.clear();
    if (collisionManager) {
        collisionManager->clearImpacts();
    }

    // Load background configuration
    if (backgroundManager) {
        backgroundManager->loadFromJson(levelData, this);
    }

    // Set Engine instance so objects can access it
    Object::setEngine(this);

    // Load objects from JSON using the component library
    if (!levelData.contains("objects") || !levelData["objects"].is_array()) {
        std::cerr << "Error: JSON file must contain an 'objects' array" << std::endl;
        return;
    }

    for (const auto& objectData : levelData["objects"]) {
        auto object = std::make_unique<Object>();
        object->fromJson(buildObjectDefinition(objectData));
        objects.push_back(std::move(object));
    }

    std::cout << "Loaded " << objects.size() << " objects from " << filename << std::endl;
}

bool Engine::saveGame(const std::string& saveFilePath) {
    bool success = SaveManager::getInstance().saveGame(this, saveFilePath);
    if (success) {
        displayMessage("Game saved");
    }
    return success;
}

bool Engine::loadGame(const std::string& saveFilePath) {
    // Check if save file exists and has level data
    if (!SaveManager::getInstance().saveExists(saveFilePath)) {
        std::cerr << "Engine::loadGame: Save file does not exist: " << saveFilePath << std::endl;
        return false;
    }
    
    // Store object count before loading
    size_t objectsBefore = objects.size();
    
    // Load the save file using loadFile (which handles save file format)
    // This loads the level data (background + objects) from the save file
    loadFile(saveFilePath);
    
    // Check if objects were actually loaded (loadFile may return early if save has no level data)
    if (objects.empty() && objectsBefore == 0) {
        // No objects were loaded - save file probably has no level data
        std::cerr << "Engine::loadGame: Save file has no level data" << std::endl;
        // Still load metadata/progress data
        SaveManager::getInstance().loadSaveData(saveFilePath);
        return false;
    }
    
    // Also load the metadata/progress data
    SaveManager::getInstance().loadSaveData(saveFilePath);
    
    return true;
}

void Engine::queueObject(std::unique_ptr<Object> object) {
    if (object) {
        pendingObjects.push_back(std::move(object));
    }
}

void Engine::mergeComponentData(nlohmann::json& baseComponent, const nlohmann::json& overrideComponent) {
    if (!overrideComponent.is_object()) {
        baseComponent = overrideComponent;
        return;
    }

    if (!baseComponent.is_object()) {
        baseComponent = overrideComponent;
        return;
    }

    for (const auto& [key, value] : overrideComponent.items()) {
        if (key == "type") {
            continue;
        }

        if (value.is_object() && baseComponent.contains(key) && baseComponent[key].is_object()) {
            Engine::mergeJsonObjects(baseComponent[key], value);
        } else {
            baseComponent[key] = value;
        }
    }
}

void Engine::mergeJsonObjects(nlohmann::json& target, const nlohmann::json& overrides) {
    if (!overrides.is_object()) {
        target = overrides;
        return;
    }

    if (!target.is_object()) {
        target = overrides;
        return;
    }

    for (const auto& [key, value] : overrides.items()) {
        if (value.is_object() && target.contains(key) && target[key].is_object()) {
            Engine::mergeJsonObjects(target[key], value);
        } else {
            target[key] = value;
        }
    }
}

nlohmann::json Engine::mergeObjectDefinitions(const nlohmann::json& baseObject, const nlohmann::json& overrides) {
    if (!overrides.is_object()) {
        return baseObject;
    }

    nlohmann::json result = baseObject.is_null() ? nlohmann::json::object() : baseObject;

    for (const auto& [key, value] : overrides.items()) {
        if (key == "components" || key == "template") {
            continue;
        }

        if (value.is_object() && result.contains(key) && result[key].is_object()) {
            Engine::mergeJsonObjects(result[key], value);
        } else {
            result[key] = value;
        }
    }

    if (overrides.contains("components") && overrides["components"].is_array()) {
        if (!result.contains("components") || !result["components"].is_array()) {
            result["components"] = nlohmann::json::array();
        }

        auto& resultComponents = result["components"];
        std::unordered_map<std::string, std::size_t> componentIndex;
        for (std::size_t i = 0; i < resultComponents.size(); ++i) {
            if (resultComponents[i].is_object() && resultComponents[i].contains("type")) {
                try {
                    componentIndex[resultComponents[i]["type"].get<std::string>()] = i;
                } catch (const std::exception&) {
                    // Skip components with non-string type
                }
            }
        }

        for (const auto& componentOverride : overrides["components"]) {
            if (!componentOverride.is_object() || !componentOverride.contains("type")) {
                resultComponents.push_back(componentOverride);
                continue;
            }

            const auto typeIt = componentOverride.find("type");
            if (!typeIt->is_string()) {
                resultComponents.push_back(componentOverride);
                continue;
            }

            const std::string typeName = typeIt->get<std::string>();
            const auto existing = componentIndex.find(typeName);
            if (existing != componentIndex.end()) {
                Engine::mergeComponentData(resultComponents[existing->second], componentOverride);
            } else {
                resultComponents.push_back(componentOverride);
                componentIndex[typeName] = resultComponents.size() - 1;
            }
        }
    }

    return result;
}

void Engine::loadObjectTemplates(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open object template file " << filename << std::endl;
        return;
    }

    try {
        nlohmann::json data;
        file >> data;
        const nlohmann::json* templatesSection = &data;
        if (data.contains("templates") && data["templates"].is_object()) {
            templatesSection = &data["templates"];
        }

        if (!templatesSection->is_object()) {
            std::cerr << "Warning: Template file '" << filename << "' must contain an object of templates" << std::endl;
            return;
        }

        objectTemplates.clear();
        for (const auto& [name, templateData] : templatesSection->items()) {
            objectTemplates[name] = templateData;
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to parse object template file '" << filename << "': " << e.what() << std::endl;
    }
}

nlohmann::json Engine::buildObjectDefinition(const nlohmann::json& objectData) const {
    if (!objectData.is_object()) {
        return objectData;
    }

    auto templateIt = objectData.find("template");
    if (templateIt == objectData.end() || !templateIt->is_string()) {
        return objectData;
    }

    const std::string templateName = templateIt->get<std::string>();
    auto found = objectTemplates.find(templateName);
    if (found == objectTemplates.end()) {
        std::cerr << "Warning: Object template '" << templateName << "' not found" << std::endl;
        nlohmann::json fallback = objectData;
        fallback.erase("template");
        return fallback;
    }

    nlohmann::json merged = mergeObjectDefinitions(found->second, objectData);
    merged.erase("template");
    return merged;
}

std::string Engine::startHosting(uint16_t hostPort, const std::string& serverManagerIP, uint16_t serverManagerPort) {
    if (hostManager) {
        stopHosting();
    }

    hostManager = std::make_unique<HostManager>(this);
    if (hostManager->Initialize(hostPort, serverManagerIP, serverManagerPort)) {
        return hostManager->GetRoomCode();
    }

    hostManager.reset();
    return "";
}

void Engine::stopHosting() {
    if (hostManager) {
        hostManager->Shutdown();
        hostManager.reset();
    }
}

bool Engine::isHosting() const {
    return hostManager && hostManager->IsHosting();
}

bool Engine::connectAsClient(const std::string& roomCode, const std::string& serverManagerIP, uint16_t serverManagerPort) {
    if (clientManager) {
        disconnectClient();
    }

    clientManager = std::make_unique<ClientManager>(this);
    if (clientManager->Connect(roomCode, serverManagerIP, serverManagerPort)) {
        return true;
    }

    clientManager.reset();
    return false;
}

void Engine::disconnectClient() {
    if (clientManager) {
        clientManager->Disconnect();
        clientManager.reset();
    }
}

bool Engine::isClient() const {
    return clientManager && clientManager->IsConnected();
}

void Engine::displayMessage(const std::string& message) {
    messageQueue.push(Message(message, DEFAULT_MESSAGE_DURATION));
    std::cout << "Engine: Queued message: " << message << std::endl;
}

void Engine::updateMessages(float deltaTime) {
    // If no current message and queue has messages, start displaying the next one
    if (!currentMessage && !messageQueue.empty()) {
        currentMessage = std::make_unique<Message>(messageQueue.front());
        messageQueue.pop();
        std::cout << "Engine: Displaying message: " << currentMessage->text << std::endl;
    }
    
    // Update current message timer
    if (currentMessage) {
        currentMessage->elapsedTime += deltaTime;
        
        // If message has been displayed long enough, clear it
        if (currentMessage->elapsedTime >= currentMessage->displayTime) {
            std::cout << "Engine: Message expired: " << currentMessage->text << std::endl;
            currentMessage.reset();
        }
    }
}

void Engine::renderMessages() {
    if (!currentMessage || !renderer) {
        return;
    }
    
    // Only render if TTF is initialized
    if (!TTF_WasInit()) {
        std::cerr << "Engine::renderMessages: TTF not initialized" << std::endl;
        return;
    }
    
    // Load font (larger for better visibility)
    TTF_Font* font = TTF_OpenFont("assets/fonts/ARIAL.TTF", 28);
    if (!font) {
        return;
    }
    
    // Render text
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface* textSurface = TTF_RenderUTF8_Blended(font, currentMessage->text.c_str(), textColor);
    if (!textSurface) {
        TTF_CloseFont(font);
        return;
    }
    
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        SDL_FreeSurface(textSurface);
        TTF_CloseFont(font);
        return;
    }
    
    // Calculate message position (anchored to bottom center)
    int marginBottom = 60;
    int textWidth = textSurface->w;
    int textHeight = textSurface->h;
    int bgPadding = 20;
    
    int bgX = (screenWidth - textWidth) / 2 - bgPadding;
    int bgY = screenHeight - marginBottom - textHeight - bgPadding;
    int bgW = textWidth + bgPadding * 2;
    int bgH = textHeight + bgPadding * 2;
    
    // Ensure we're using the correct blend mode
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    
    // Draw semi-transparent background (more opaque for better visibility)
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 240);
    SDL_Rect bgRect = {bgX, bgY, bgW, bgH};
    SDL_RenderFillRect(renderer, &bgRect);
    
    // Draw border (thicker for better visibility)
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &bgRect);
    // Draw a second border line for thickness
    SDL_Rect borderRect = {bgRect.x - 1, bgRect.y - 1, bgRect.w + 2, bgRect.h + 2};
    SDL_RenderDrawRect(renderer, &borderRect);
    
    // Draw text with proper blend mode
    SDL_SetTextureBlendMode(textTexture, SDL_BLENDMODE_BLEND);
    SDL_Rect textRect;
    textRect.x = bgX + bgPadding;
    textRect.y = bgY + bgPadding;
    textRect.w = textWidth;
    textRect.h = textHeight;
    SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
    
    // Cleanup
    SDL_DestroyTexture(textTexture);
    SDL_FreeSurface(textSurface);
    TTF_CloseFont(font);
}