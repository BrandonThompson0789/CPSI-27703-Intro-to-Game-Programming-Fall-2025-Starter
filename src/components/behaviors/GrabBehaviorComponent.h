#pragma once

#include "../Component.h"
#include "../InputComponent.h"
#include "../BodyComponent.h"
#include <nlohmann/json.hpp>

class JointComponent;
class SoundComponent;

/**
 * Handles interacting with physics objects: grabbing, holding, and releasing
 * objects in front of the player. An optional density limit can prevent
 * grabbing very heavy objects.
 */
class GrabBehaviorComponent : public Component {
public:
    GrabBehaviorComponent(Object& parent);
    GrabBehaviorComponent(Object& parent, const nlohmann::json& data);
    ~GrabBehaviorComponent() override = default;

    void update(float deltaTime) override;
    void draw() override {}

    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "GrabBehaviorComponent"; }

    Object* getGrabbedObject() const { return grabbedObject; }
    bool hasGrabbedObject() const { return grabbedObject != nullptr; }
    JointComponent* getGrabJoint() const { return grabJoint; }

    void releaseGrabbedObject(bool playSound = true);
    Object* detachGrabbedObject(bool playSound = true);

    void setGrabDistance(float distance) { grabDistance = distance; }
    float getGrabDistance() const { return grabDistance; }

    void setMaxGrabDensity(float density) { maxGrabDensity = density; }
    float getMaxGrabDensity() const { return maxGrabDensity; }

    void setGrabForce(float force) { grabForce = force; }
    void setBreakForce(float force) { breakForce = force; }

private:
    void resolveDependencies();
    void handleInteractInput();
    void maintainGrabbedObjectOffset();
    void autoReleaseIfNecessary();

    Object* findGrabbableObject();
    bool isGrabbable(Object* candidate) const;
    bool passesDensityLimit(BodyComponent* targetBody) const;
    void grabObject(Object* obj);

    InputComponent* input;
    BodyComponent* body;

    Object* grabbedObject;
    JointComponent* grabJoint;
    SoundComponent* sound;
    float grabDistance;
    float grabForce;
    float breakForce;
    float maxGrabDensity;
    bool wasInteractPressed;
};


