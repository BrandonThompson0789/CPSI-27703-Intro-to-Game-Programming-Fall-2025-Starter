#pragma once

#include "Object.h"
#include <nlohmann/json.hpp>
#include <string>
#include <fstream>
#include <vector>

/**
 * ObjectSerializer - Utility class for saving and loading Objects to/from JSON files
 * 
 * Example usage:
 * 
 * // Save an object
 * Object myObject;
 * myObject.addComponent<BodyComponent>();
 * myObject.addComponent<SpriteComponent>("player", true, true);
 * ObjectSerializer::saveObjectToFile(myObject, "saved_object.json");
 * 
 * // Load an object
 * Object loadedObject;
 * ObjectSerializer::loadObjectFromFile(loadedObject, "saved_object.json");
 * 
 * // Save multiple objects
 * std::vector<Object> objects;
 * ObjectSerializer::saveObjectsToFile(objects, "saved_objects.json");
 * 
 * // Load multiple objects
 * std::vector<Object> loadedObjects;
 * ObjectSerializer::loadObjectsFromFile(loadedObjects, "saved_objects.json");
 */
class ObjectSerializer {
public:
    /**
     * Save a single object to a JSON file
     */
    static bool saveObjectToFile(const Object& object, const std::string& filename) {
        try {
            nlohmann::json j = object.toJson();
            
            std::ofstream file(filename);
            if (!file.is_open()) {
                return false;
            }
            
            file << j.dump(4); // Pretty print with 4 space indent
            file.close();
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    /**
     * Load a single object from a JSON file
     */
    static bool loadObjectFromFile(Object& object, const std::string& filename) {
        try {
            std::ifstream file(filename);
            if (!file.is_open()) {
                return false;
            }
            
            nlohmann::json j;
            file >> j;
            file.close();
            
            object.fromJson(j);
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    /**
     * Save multiple objects to a JSON file
     */
    static bool saveObjectsToFile(const std::vector<Object*>& objects, const std::string& filename) {
        try {
            nlohmann::json j = nlohmann::json::array();
            
            for (const auto* object : objects) {
                if (object) {
                    j.push_back(object->toJson());
                }
            }
            
            std::ofstream file(filename);
            if (!file.is_open()) {
                return false;
            }
            
            file << j.dump(4);
            file.close();
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    /**
     * Load multiple objects from a JSON file
     * Note: Creates new objects and adds them to the vector
     */
    static bool loadObjectsFromFile(std::vector<std::unique_ptr<Object>>& objects, const std::string& filename) {
        try {
            std::ifstream file(filename);
            if (!file.is_open()) {
                return false;
            }
            
            nlohmann::json j;
            file >> j;
            file.close();
            
            if (!j.is_array()) {
                return false;
            }
            
            objects.clear();
            
            for (const auto& objectData : j) {
                auto object = std::make_unique<Object>();
                object->fromJson(objectData);
                objects.push_back(std::move(object));
            }
            
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    /**
     * Convert an object to a JSON string
     */
    static std::string objectToString(const Object& object, int indent = -1) {
        nlohmann::json j = object.toJson();
        return j.dump(indent);
    }
    
    /**
     * Load an object from a JSON string
     */
    static bool objectFromString(Object& object, const std::string& jsonString) {
        try {
            nlohmann::json j = nlohmann::json::parse(jsonString);
            object.fromJson(j);
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
};

