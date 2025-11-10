#ifndef OBJECT_H
#define OBJECT_H

#include <SDL.h>
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <nlohmann/json.hpp>

class Component;
class Engine;

class Object {
    public:
        Object();
        virtual ~Object();
        
        void update(float deltaTime = 1.0f / 60.0f);
        void render(SDL_Renderer* renderer);
        
        // Name management
        void setName(const std::string& n) { name = n; }
        const std::string& getName() const { return name; }
        
        // Component management
        template<typename T, typename... Args>
        T* addComponent(Args&&... args) {
            auto component = std::make_unique<T>(*this, std::forward<Args>(args)...);
            T* ptr = component.get();
            components.push_back(std::move(component));
            componentMap[std::type_index(typeid(T))] = ptr;
            return ptr;
        }
        
        template<typename T>
        T* getComponent() {
            auto it = componentMap.find(std::type_index(typeid(T)));
            if (it != componentMap.end()) {
                return static_cast<T*>(it->second);
            }
            return nullptr;
        }
        
        template<typename T>
        const T* getComponent() const {
            auto it = componentMap.find(std::type_index(typeid(T)));
            if (it != componentMap.end()) {
                return static_cast<T*>(it->second);
            }
            return nullptr;
        }

        template<typename T>
        bool hasComponent() const {
            return componentMap.find(std::type_index(typeid(T))) != componentMap.end();
        }
        
        // Serialization
        nlohmann::json toJson() const;
        void fromJson(const nlohmann::json& data);
        
        // Static method to set/get Engine instance
        static void setEngine(Engine* engine);
        static Engine* getEngine();
        static bool isAlive(const Object* object);

        // Lifecycle management
        void markForDeath();
        bool isMarkedForDeath() const;
        
    private:
        std::string name;
        std::vector<std::unique_ptr<Component>> components;
        std::unordered_map<std::type_index, Component*> componentMap;
        static Engine* engineInstance;
        static std::unordered_set<Object*> liveObjects;
        bool markedForDeath = false;
};

#endif // OBJECT_H