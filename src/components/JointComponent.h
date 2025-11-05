#pragma once
#include "Component.h"
#include <box2d/box2d.h>
#include <nlohmann/json.hpp>
#include <string>

class Object;

/**
 * JointComponent - Manages Box2D v3.x joints between objects
 * 
 * Supports all Box2D joint types:
 * - Distance: Maintains distance between two bodies
 * - Revolute: Rotation around a point (like a hinge)
 * - Prismatic: Sliding along an axis
 * - Weld: Rigid connection
 * - Wheel: Suspension joint
 * - Motor: Controls relative motion
 * - Mouse: Makes a point track a world position
 * - Filter: Disables collision between bodies
 * 
 * Features:
 * - JSON serialization/deserialization
 * - Joint breaking based on force, torque, or separation limits
 * - Connects to child objects by name or direct reference
 */
class JointComponent : public Component {
public:
    // Constructors
    JointComponent(Object& parent);
    JointComponent(Object& parent, const nlohmann::json& data);
    ~JointComponent() override;
    
    // Component interface
    void update(float deltaTime) override;
    void draw() override;
    
    // Serialization
    nlohmann::json toJson() const override;
    std::string getTypeName() const override { return "JointComponent"; }
    
    // Joint management
    void createDistanceJoint(Object* bodyB, const b2Vec2& anchorA, const b2Vec2& anchorB,
                            float length = -1.0f, float hertz = 0.0f, float dampingRatio = 0.0f);
    
    void createRevoluteJoint(Object* bodyB, const b2Vec2& anchorA, const b2Vec2& anchorB,
                            bool enableLimits = false, float lowerAngle = 0.0f, float upperAngle = 0.0f,
                            bool enableMotor = false, float motorSpeed = 0.0f, float maxMotorTorque = 0.0f,
                            bool enableSpring = false, float targetAngle = 0.0f, float hertz = 0.0f, float dampingRatio = 0.0f);
    
    void createPrismaticJoint(Object* bodyB, const b2Vec2& anchorA, const b2Vec2& anchorB, const b2Vec2& axis,
                             bool enableLimits = false, float lowerTranslation = 0.0f, float upperTranslation = 0.0f,
                             bool enableMotor = false, float motorSpeed = 0.0f, float maxMotorForce = 0.0f);
    
    void createWeldJoint(Object* bodyB, const b2Vec2& anchorA, const b2Vec2& anchorB,
                        float hertz = 0.0f, float dampingRatio = 0.0f);
    
    void createWheelJoint(Object* bodyB, const b2Vec2& anchorA, const b2Vec2& anchorB, const b2Vec2& axis,
                         float hertz = 0.0f, float dampingRatio = 0.0f,
                         bool enableMotor = false, float motorSpeed = 0.0f, float maxMotorTorque = 0.0f);
    
    void createMotorJoint(Object* bodyB, const b2Vec2& linearOffset = {0.0f, 0.0f},
                         float angularOffset = 0.0f, float maxForce = 1.0f, float maxTorque = 1.0f);
    
    void createMouseJoint(Object* bodyB, const b2Vec2& target,
                         float hertz = 5.0f, float dampingRatio = 0.7f, float maxForce = 1000.0f);
    
    void createFilterJoint(Object* bodyB);
    
    // Breaking limits
    void setBreakForce(float force) { maxBreakForce = force; enableBreaking = true; }
    void setBreakTorque(float torque) { maxBreakTorque = torque; enableBreaking = true; }
    void setBreakSeparation(float separation) { maxBreakSeparation = separation; enableBreaking = true; }
    
    // Query methods
    bool isJointBroken() const { return jointBroken; }
    b2JointType getJointType() const;
    b2Vec2 getConstraintForce() const;
    float getConstraintTorque() const;
    float getLinearSeparation() const;
    float getAngularSeparation() const;
    
    // Get the joint ID (for advanced usage)
    b2JointId getJointId() const { return jointId; }
    
    // Manually destroy the joint (useful for releasing grabbed objects)
    void destroyJoint();
    
private:
    b2JointId jointId;
    Object* connectedBody; // The object this joint connects to
    std::string connectedBodyName; // Name of connected body (for JSON loading)
    
    // Breaking limits
    bool enableBreaking;
    float maxBreakForce;     // Maximum constraint force before breaking (Newtons)
    float maxBreakTorque;    // Maximum constraint torque before breaking (Newton-meters)
    float maxBreakSeparation; // Maximum linear separation before breaking (meters)
    bool jointBroken;
    
    // Helper methods
    void createJointFromJson(const nlohmann::json& data);
    void checkBreakingLimits();
    Object* findObjectByName(const std::string& name);
    std::string jointTypeToString(b2JointType type) const;
    
    // Conversion helpers
    b2Vec2 pixelsToMeters(const b2Vec2& pixels) const;
    b2Vec2 metersToPixels(const b2Vec2& meters) const;
};

