#ifndef LIBRARY_H
#define LIBRARY_H

#include <unordered_map>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

// Forward declarations
class Object;
class Player;
class Enemy;
class Item;

class Library {
public:
    std::unordered_map<
        std::string, 
        std::function<std::unique_ptr<Object>(const nlohmann::json&)>
    > map;
    
    // Constructor registers all object types
    Library() {
        // Register Player
        map["player"] = [](const nlohmann::json& data) -> std::unique_ptr<Object> {
            return std::make_unique<Player>(data);
        };
        
        // Register Enemy
        map["enemy"] = [](const nlohmann::json& data) -> std::unique_ptr<Object> {
            return std::make_unique<Enemy>(data);
        };
        
        // Register Item
        map["item"] = [](const nlohmann::json& data) -> std::unique_ptr<Object> {
            return std::make_unique<Item>(data);
        };
    }
    
    // Static method to get the global Library instance
    static Library& getLibrary() {
        static Library library;
        return library;
    }
};

#endif // LIBRARY_H