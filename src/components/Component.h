#pragma once
#include <typeindex>

class Object;

class Component {
public:
    Component(Object& parent) : parentObject(parent) {}

    virtual void update()=0;
    virtual void draw() =0;

    virtual ~Component() = default;

protected:
    Object& parent() const { return parentObject; }

private:
    Object& parentObject;
};
