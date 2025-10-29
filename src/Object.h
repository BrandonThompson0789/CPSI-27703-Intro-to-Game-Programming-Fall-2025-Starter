#ifndef OBJECT_H
#define OBJECT_H

#include <SDL.h>
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <nlohmann/json.hpp>

class Component;

class Object {
    public:
        Object() = default;
        virtual ~Object();
        
        void update(float deltaTime = 1.0f / 60.0f);
        void render(SDL_Renderer* renderer);
        
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
        bool hasComponent() const {
            return componentMap.find(std::type_index(typeid(T))) != componentMap.end();
        }
        
        // Serialization
        nlohmann::json toJson() const;
        void fromJson(const nlohmann::json& data);
        
    private:
        std::vector<std::unique_ptr<Component>> components;
        std::unordered_map<std::type_index, Component*> componentMap;
};

#endif // OBJECT_H