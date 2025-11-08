#pragma once
#include <typeindex>
#include <string>
#include <nlohmann/json.hpp>

class Object;

class Component {
public:
    Component(Object& parent) : parentObject(parent) {}

    virtual void update(float deltaTime = 1.0f / 60.0f)=0;
    virtual void draw() =0;

    virtual ~Component() = default;
    
    // Serialization methods
    virtual nlohmann::json toJson() const = 0;
    virtual std::string getTypeName() const = 0;

    // Lifecycle hooks
    virtual void onParentDeath() {}

protected:
    Object& parent() const { return parentObject; }

private:
    Object& parentObject;
};
