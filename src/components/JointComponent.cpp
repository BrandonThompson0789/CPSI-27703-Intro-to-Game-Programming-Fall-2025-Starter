#include "JointComponent.h"
#include "ComponentLibrary.h"
#include "BodyComponent.h"
#include "../Engine.h"
#include "../Object.h"
#include <iostream>
#include <cmath>

JointComponent::JointComponent(Object& parent) 
    : Component(parent), 
      jointId(b2_nullJointId),
      connectedBody(nullptr),
      enableBreaking(false),
      maxBreakForce(INFINITY),
      maxBreakTorque(INFINITY),
      maxBreakSeparation(INFINITY),
      jointBroken(false) {
}

JointComponent::JointComponent(Object& parent, const nlohmann::json& data) 
    : Component(parent),
      jointId(b2_nullJointId),
      connectedBody(nullptr),
      enableBreaking(false),
      maxBreakForce(INFINITY),
      maxBreakTorque(INFINITY),
      maxBreakSeparation(INFINITY),
      jointBroken(false) {
    createJointFromJson(data);
}

JointComponent::~JointComponent() {
    destroyJoint();
}

void JointComponent::setBreakSeparation(float separation) {
    maxBreakSeparation = separation * Engine::PIXELS_TO_METERS;
    enableBreaking = true;
}

void JointComponent::destroyJoint() {
    if (B2_IS_NON_NULL(jointId)) {
        b2DestroyJoint(jointId);
        jointId = b2_nullJointId;
    }
    // Reset broken flag so joint can be recreated
    jointBroken = false;
    connectedBody = nullptr;
}

nlohmann::json JointComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    
    if (B2_IS_NULL(jointId)) return j;
    
    // Store joint type
    b2JointType type = b2Joint_GetType(jointId);
    j["jointType"] = jointTypeToString(type);
    
    // Store connected body name (if we have one)
    if (!connectedBodyName.empty()) {
        j["connectedBody"] = connectedBodyName;
    }
    
    // Store anchors (in pixels)
    b2Vec2 anchorA = b2Joint_GetLocalAnchorA(jointId);
    b2Vec2 anchorB = b2Joint_GetLocalAnchorB(jointId);
    j["anchorA"] = {metersToPixels(anchorA).x, metersToPixels(anchorA).y};
    j["anchorB"] = {metersToPixels(anchorB).x, metersToPixels(anchorB).y};
    
    // Store collision state
    j["collideConnected"] = b2Joint_GetCollideConnected(jointId);
    
    // Store breaking limits
    if (enableBreaking) {
        j["enableBreaking"] = true;
        if (maxBreakForce != INFINITY) {
            j["maxBreakForce"] = maxBreakForce;
        }
        if (maxBreakTorque != INFINITY) {
            j["maxBreakTorque"] = maxBreakTorque;
        }
        if (maxBreakSeparation != INFINITY) {
            j["maxBreakSeparation"] = maxBreakSeparation * Engine::METERS_TO_PIXELS;
        }
    }
    
    // Type-specific properties
    switch (type) {
        case b2_distanceJoint: {
            j["length"] = b2DistanceJoint_GetLength(jointId) * Engine::METERS_TO_PIXELS;
            j["springEnabled"] = b2DistanceJoint_IsSpringEnabled(jointId);
            if (b2DistanceJoint_IsSpringEnabled(jointId)) {
                j["hertz"] = b2DistanceJoint_GetSpringHertz(jointId);
                j["dampingRatio"] = b2DistanceJoint_GetSpringDampingRatio(jointId);
            }
            break;
        }
        case b2_revoluteJoint: {
            j["referenceAngle"] = Engine::radiansToDegrees(b2Joint_GetReferenceAngle(jointId));
            j["enableLimit"] = b2RevoluteJoint_IsLimitEnabled(jointId);
            if (b2RevoluteJoint_IsLimitEnabled(jointId)) {
                j["lowerAngle"] = Engine::radiansToDegrees(b2RevoluteJoint_GetLowerLimit(jointId));
                j["upperAngle"] = Engine::radiansToDegrees(b2RevoluteJoint_GetUpperLimit(jointId));
            }
            j["enableMotor"] = b2RevoluteJoint_IsMotorEnabled(jointId);
            if (b2RevoluteJoint_IsMotorEnabled(jointId)) {
                j["motorSpeed"] = Engine::radiansToDegrees(b2RevoluteJoint_GetMotorSpeed(jointId));
                j["maxMotorTorque"] = b2RevoluteJoint_GetMaxMotorTorque(jointId);
            }
            j["enableSpring"] = b2RevoluteJoint_IsSpringEnabled(jointId);
            if (b2RevoluteJoint_IsSpringEnabled(jointId)) {
                j["targetAngle"] = Engine::radiansToDegrees(b2RevoluteJoint_GetTargetAngle(jointId));
                j["hertz"] = b2RevoluteJoint_GetSpringHertz(jointId);
                j["dampingRatio"] = b2RevoluteJoint_GetSpringDampingRatio(jointId);
            }
            break;
        }
        case b2_prismaticJoint: {
            b2Vec2 axis = b2Joint_GetLocalAxisA(jointId);
            j["axis"] = {axis.x, axis.y};
            j["referenceAngle"] = b2Joint_GetReferenceAngle(jointId);
            j["enableLimit"] = b2PrismaticJoint_IsLimitEnabled(jointId);
            if (b2PrismaticJoint_IsLimitEnabled(jointId)) {
                j["lowerTranslation"] = b2PrismaticJoint_GetLowerLimit(jointId) * Engine::METERS_TO_PIXELS;
                j["upperTranslation"] = b2PrismaticJoint_GetUpperLimit(jointId) * Engine::METERS_TO_PIXELS;
            }
            j["enableMotor"] = b2PrismaticJoint_IsMotorEnabled(jointId);
            if (b2PrismaticJoint_IsMotorEnabled(jointId)) {
                j["motorSpeed"] = b2PrismaticJoint_GetMotorSpeed(jointId) * Engine::METERS_TO_PIXELS;
                j["maxMotorForce"] = b2PrismaticJoint_GetMaxMotorForce(jointId);
            }
            break;
        }
        case b2_weldJoint: {
            j["referenceAngle"] = Engine::radiansToDegrees(b2Joint_GetReferenceAngle(jointId));
            float hertz, dampingRatio;
            b2Joint_GetConstraintTuning(jointId, &hertz, &dampingRatio);
            j["hertz"] = hertz;
            j["dampingRatio"] = dampingRatio;
            break;
        }
        case b2_wheelJoint: {
            b2Vec2 axis = b2Joint_GetLocalAxisA(jointId);
            j["axis"] = {axis.x, axis.y};
            j["enableSpring"] = b2WheelJoint_IsSpringEnabled(jointId);
            if (b2WheelJoint_IsSpringEnabled(jointId)) {
                j["hertz"] = b2WheelJoint_GetSpringHertz(jointId);
                j["dampingRatio"] = b2WheelJoint_GetSpringDampingRatio(jointId);
            }
            j["enableMotor"] = b2WheelJoint_IsMotorEnabled(jointId);
            if (b2WheelJoint_IsMotorEnabled(jointId)) {
                j["motorSpeed"] = Engine::radiansToDegrees(b2WheelJoint_GetMotorSpeed(jointId));
                j["maxMotorTorque"] = b2WheelJoint_GetMaxMotorTorque(jointId);
            }
            break;
        }
        case b2_motorJoint: {
            b2Vec2 offset = b2MotorJoint_GetLinearOffset(jointId);
            j["linearOffset"] = {metersToPixels(offset).x, metersToPixels(offset).y};
            j["angularOffset"] = Engine::radiansToDegrees(b2MotorJoint_GetAngularOffset(jointId));
            j["maxForce"] = b2MotorJoint_GetMaxForce(jointId);
            j["maxTorque"] = b2MotorJoint_GetMaxTorque(jointId);
            break;
        }
        case b2_mouseJoint: {
            b2Vec2 target = b2MouseJoint_GetTarget(jointId);
            j["target"] = {metersToPixels(target).x, metersToPixels(target).y};
            j["hertz"] = b2MouseJoint_GetSpringHertz(jointId);
            j["dampingRatio"] = b2MouseJoint_GetSpringDampingRatio(jointId);
            j["maxForce"] = b2MouseJoint_GetMaxForce(jointId);
            break;
        }
        default:
            break;
    }
    
    return j;
}

// Register this component type with the library
static ComponentRegistrar<JointComponent> registrar("JointComponent");

void JointComponent::update(float deltaTime) {
    // If joint is broken, nothing to do
    if (jointBroken || B2_IS_NULL(jointId)) {
        return;
    }
    
    // Check breaking limits if enabled
    if (enableBreaking) {
        checkBreakingLimits();
    }
}

void JointComponent::draw() {
    // JointComponent doesn't render anything itself
    // Box2D debug draw can visualize joints if needed
}

void JointComponent::checkBreakingLimits() {
    if (B2_IS_NULL(jointId)) return;
    
    bool shouldBreak = false;
    
    // Check force limit
    if (maxBreakForce != INFINITY) {
        b2Vec2 force = b2Joint_GetConstraintForce(jointId);
        float forceMagnitude = std::sqrt(force.x * force.x + force.y * force.y);
        if (forceMagnitude > maxBreakForce) {
            shouldBreak = true;
        }
    }
    
    // Check torque limit
    if (maxBreakTorque != INFINITY) {
        float torque = std::abs(b2Joint_GetConstraintTorque(jointId));
        if (torque > maxBreakTorque) {
            shouldBreak = true;
        }
    }
    
    // Check separation limit
    if (maxBreakSeparation != INFINITY) {
        float separation = std::abs(b2Joint_GetLinearSeparation(jointId));
        if (separation > maxBreakSeparation) {
            shouldBreak = true;
        }
    }
    
    if (shouldBreak) {
        jointBroken = true;
        destroyJoint();
    }
}

// Helper to find an object by name
Object* JointComponent::findObjectByName(const std::string& name) {
    Engine* engine = Object::getEngine();
    if (!engine) return nullptr;
    
    for (auto& obj : engine->getObjects()) {
        if (obj->getName() == name) {
            return obj.get();
        }
    }
    
    return nullptr;
}

void JointComponent::createJointFromJson(const nlohmann::json& data) {
    // Parse joint type
    std::string jointTypeStr = data.value("jointType", "distance");
    
    // Get connected body name
    connectedBodyName = data.value("connectedBody", "");
    
    // Find connected body (this might fail if objects aren't loaded yet)
    // In a real implementation, you might need deferred joint creation
    connectedBody = findObjectByName(connectedBodyName);
    
    if (!connectedBody) {
        std::cerr << "Warning: Connected body '" << connectedBodyName 
                  << "' not found. Joint will not be created." << std::endl;
        // Note: In a production system, you'd want to defer joint creation
        // until all objects are loaded
        return;
    }
    
    // Parse anchors (in pixels, convert to meters)
    b2Vec2 anchorA = {0.0f, 0.0f};
    b2Vec2 anchorB = {0.0f, 0.0f};
    
    if (data.contains("anchorA") && data["anchorA"].is_array() && data["anchorA"].size() == 2) {
        anchorA.x = data["anchorA"][0].get<float>() * Engine::PIXELS_TO_METERS;
        anchorA.y = data["anchorA"][1].get<float>() * Engine::PIXELS_TO_METERS;
    }
    
    if (data.contains("anchorB") && data["anchorB"].is_array() && data["anchorB"].size() == 2) {
        anchorB.x = data["anchorB"][0].get<float>() * Engine::PIXELS_TO_METERS;
        anchorB.y = data["anchorB"][1].get<float>() * Engine::PIXELS_TO_METERS;
    }
    
    // Parse breaking limits
    if (data.value("enableBreaking", false)) {
        enableBreaking = true;
        maxBreakForce = data.value("maxBreakForce", INFINITY);
        maxBreakTorque = data.value("maxBreakTorque", INFINITY);
        maxBreakSeparation = data.value("maxBreakSeparation", INFINITY);
        if (maxBreakSeparation != INFINITY) {
            maxBreakSeparation *= Engine::PIXELS_TO_METERS;
        }
    }
    
    // Create joint based on type
    if (jointTypeStr == "distance") {
        float length = data.value("length", -1.0f);
        if (length > 0) length *= Engine::PIXELS_TO_METERS;
        float hertz = data.value("hertz", 0.0f);
        float dampingRatio = data.value("dampingRatio", 0.0f);
        createDistanceJoint(connectedBody, anchorA, anchorB, length, hertz, dampingRatio);
    }
    else if (jointTypeStr == "revolute") {
        bool enableLimit = data.value("enableLimit", false);
        float lowerAngle = data.value("lowerAngle", 0.0f);
        float upperAngle = data.value("upperAngle", 0.0f);
        bool enableMotor = data.value("enableMotor", false);
        float motorSpeed = data.value("motorSpeed", 0.0f);
        float maxMotorTorque = data.value("maxMotorTorque", 0.0f);
        bool enableSpring = data.value("enableSpring", false);
        float targetAngle = data.value("targetAngle", 0.0f);
        float hertz = data.value("hertz", 0.0f);
        float dampingRatio = data.value("dampingRatio", 0.0f);
        createRevoluteJoint(connectedBody, anchorA, anchorB, enableLimit, lowerAngle, upperAngle,
                          enableMotor, motorSpeed, maxMotorTorque, enableSpring, targetAngle, hertz, dampingRatio);
    }
    else if (jointTypeStr == "prismatic") {
        b2Vec2 axis = {1.0f, 0.0f}; // default to horizontal
        if (data.contains("axis") && data["axis"].is_array() && data["axis"].size() == 2) {
            axis.x = data["axis"][0].get<float>();
            axis.y = data["axis"][1].get<float>();
        }
        bool enableLimit = data.value("enableLimit", false);
        float lowerTranslation = data.value("lowerTranslation", 0.0f) * Engine::PIXELS_TO_METERS;
        float upperTranslation = data.value("upperTranslation", 0.0f) * Engine::PIXELS_TO_METERS;
        bool enableMotor = data.value("enableMotor", false);
        float motorSpeed = data.value("motorSpeed", 0.0f) * Engine::PIXELS_TO_METERS;
        float maxMotorForce = data.value("maxMotorForce", 0.0f);
        createPrismaticJoint(connectedBody, anchorA, anchorB, axis, enableLimit, lowerTranslation, upperTranslation,
                           enableMotor, motorSpeed, maxMotorForce);
    }
    else if (jointTypeStr == "weld") {
        float hertz = data.value("hertz", 0.0f);
        float dampingRatio = data.value("dampingRatio", 0.0f);
        createWeldJoint(connectedBody, anchorA, anchorB, hertz, dampingRatio);
    }
    else if (jointTypeStr == "wheel") {
        b2Vec2 axis = {0.0f, 1.0f}; // default to vertical
        if (data.contains("axis") && data["axis"].is_array() && data["axis"].size() == 2) {
            axis.x = data["axis"][0].get<float>();
            axis.y = data["axis"][1].get<float>();
        }
        float hertz = data.value("hertz", 0.0f);
        float dampingRatio = data.value("dampingRatio", 0.0f);
        bool enableMotor = data.value("enableMotor", false);
        float motorSpeed = data.value("motorSpeed", 0.0f);
        float maxMotorTorque = data.value("maxMotorTorque", 0.0f);
        createWheelJoint(connectedBody, anchorA, anchorB, axis, hertz, dampingRatio,
                        enableMotor, motorSpeed, maxMotorTorque);
    }
    else if (jointTypeStr == "motor") {
        b2Vec2 linearOffset = {0.0f, 0.0f};
        if (data.contains("linearOffset") && data["linearOffset"].is_array() && data["linearOffset"].size() == 2) {
            linearOffset.x = data["linearOffset"][0].get<float>() * Engine::PIXELS_TO_METERS;
            linearOffset.y = data["linearOffset"][1].get<float>() * Engine::PIXELS_TO_METERS;
        }
        float angularOffset = data.value("angularOffset", 0.0f);
        float maxForce = data.value("maxForce", 1.0f);
        float maxTorque = data.value("maxTorque", 1.0f);
        createMotorJoint(connectedBody, linearOffset, angularOffset, maxForce, maxTorque);
    }
    else if (jointTypeStr == "mouse") {
        b2Vec2 target = {0.0f, 0.0f};
        if (data.contains("target") && data["target"].is_array() && data["target"].size() == 2) {
            target.x = data["target"][0].get<float>() * Engine::PIXELS_TO_METERS;
            target.y = data["target"][1].get<float>() * Engine::PIXELS_TO_METERS;
        }
        float hertz = data.value("hertz", 5.0f);
        float dampingRatio = data.value("dampingRatio", 0.7f);
        float maxForce = data.value("maxForce", 1000.0f);
        createMouseJoint(connectedBody, target, hertz, dampingRatio, maxForce);
    }
    else if (jointTypeStr == "filter") {
        createFilterJoint(connectedBody);
    }
    
    // Set collision state if specified
    if (data.contains("collideConnected") && B2_IS_NON_NULL(jointId)) {
        b2Joint_SetCollideConnected(jointId, data["collideConnected"].get<bool>());
    }
}

// Joint creation methods
void JointComponent::createDistanceJoint(Object* bodyB, const b2Vec2& anchorA, const b2Vec2& anchorB,
                                        float length, float hertz, float dampingRatio) {
    if (jointBroken || B2_IS_NON_NULL(jointId)) {
        std::cerr << "Warning: Joint already exists or is broken" << std::endl;
        return;
    }
    
    Engine* engine = Object::getEngine();
    if (!engine) return;
    
    BodyComponent* bodyCompA = parent().getComponent<BodyComponent>();
    BodyComponent* bodyCompB = bodyB ? bodyB->getComponent<BodyComponent>() : nullptr;
    
    if (!bodyCompA || !bodyCompB) {
        std::cerr << "Error: Both objects must have BodyComponent" << std::endl;
        return;
    }
    
    b2DistanceJointDef def = b2DefaultDistanceJointDef();
    def.bodyIdA = bodyCompA->getBodyId();
    def.bodyIdB = bodyCompB->getBodyId();
    def.localAnchorA = anchorA;
    def.localAnchorB = anchorB;
    
    if (length > 0) {
        def.length = length;
    }
    
    if (hertz > 0.0f) {
        def.enableSpring = true;
        def.hertz = hertz;
        def.dampingRatio = dampingRatio;
    }
    
    jointId = b2CreateDistanceJoint(engine->getPhysicsWorld(), &def);
    connectedBody = bodyB;
}

void JointComponent::createRevoluteJoint(Object* bodyB, const b2Vec2& anchorA, const b2Vec2& anchorB,
                                        bool enableLimits, float lowerAngle, float upperAngle,
                                        bool enableMotor, float motorSpeed, float maxMotorTorque,
                                        bool enableSpring, float targetAngle, float hertz, float dampingRatio) {
    if (jointBroken || B2_IS_NON_NULL(jointId)) {
        std::cerr << "Warning: Joint already exists or is broken" << std::endl;
        return;
    }
    
    Engine* engine = Object::getEngine();
    if (!engine) return;
    
    BodyComponent* bodyCompA = parent().getComponent<BodyComponent>();
    BodyComponent* bodyCompB = bodyB ? bodyB->getComponent<BodyComponent>() : nullptr;
    
    if (!bodyCompA || !bodyCompB) {
        std::cerr << "Error: Both objects must have BodyComponent" << std::endl;
        return;
    }
    
    b2RevoluteJointDef def = b2DefaultRevoluteJointDef();
    def.bodyIdA = bodyCompA->getBodyId();
    def.bodyIdB = bodyCompB->getBodyId();
    def.localAnchorA = anchorA;
    def.localAnchorB = anchorB;
    
    if (enableLimits) {
        def.enableLimit = true;
        def.lowerAngle = Engine::degreesToRadians(lowerAngle);
        def.upperAngle = Engine::degreesToRadians(upperAngle);
    }
    
    if (enableMotor) {
        def.enableMotor = true;
        def.motorSpeed = Engine::degreesToRadians(motorSpeed);
        def.maxMotorTorque = maxMotorTorque;
    }
    
    if (enableSpring) {
        def.enableSpring = true;
        def.targetAngle = Engine::degreesToRadians(targetAngle);
        def.hertz = hertz;
        def.dampingRatio = dampingRatio;
    }
    
    jointId = b2CreateRevoluteJoint(engine->getPhysicsWorld(), &def);
    connectedBody = bodyB;
}

void JointComponent::createPrismaticJoint(Object* bodyB, const b2Vec2& anchorA, const b2Vec2& anchorB, const b2Vec2& axis,
                                         bool enableLimits, float lowerTranslation, float upperTranslation,
                                         bool enableMotor, float motorSpeed, float maxMotorForce) {
    if (jointBroken || B2_IS_NON_NULL(jointId)) {
        std::cerr << "Warning: Joint already exists or is broken" << std::endl;
        return;
    }
    
    Engine* engine = Object::getEngine();
    if (!engine) return;
    
    BodyComponent* bodyCompA = parent().getComponent<BodyComponent>();
    BodyComponent* bodyCompB = bodyB ? bodyB->getComponent<BodyComponent>() : nullptr;
    
    if (!bodyCompA || !bodyCompB) {
        std::cerr << "Error: Both objects must have BodyComponent" << std::endl;
        return;
    }
    
    b2PrismaticJointDef def = b2DefaultPrismaticJointDef();
    def.bodyIdA = bodyCompA->getBodyId();
    def.bodyIdB = bodyCompB->getBodyId();
    def.localAnchorA = anchorA;
    def.localAnchorB = anchorB;
    def.localAxisA = axis;
    
    if (enableLimits) {
        def.enableLimit = true;
        def.lowerTranslation = lowerTranslation;
        def.upperTranslation = upperTranslation;
    }
    
    if (enableMotor) {
        def.enableMotor = true;
        def.motorSpeed = motorSpeed;
        def.maxMotorForce = maxMotorForce;
    }
    
    jointId = b2CreatePrismaticJoint(engine->getPhysicsWorld(), &def);
    connectedBody = bodyB;
}

void JointComponent::createWeldJoint(Object* bodyB, const b2Vec2& anchorA, const b2Vec2& anchorB,
                                    float hertz, float dampingRatio) {
    if (jointBroken || B2_IS_NON_NULL(jointId)) {
        std::cerr << "Warning: Joint already exists or is broken" << std::endl;
        return;
    }
    
    Engine* engine = Object::getEngine();
    if (!engine) return;
    
    BodyComponent* bodyCompA = parent().getComponent<BodyComponent>();
    BodyComponent* bodyCompB = bodyB ? bodyB->getComponent<BodyComponent>() : nullptr;
    
    if (!bodyCompA || !bodyCompB) {
        std::cerr << "Error: Both objects must have BodyComponent" << std::endl;
        return;
    }
    
    b2WeldJointDef def = b2DefaultWeldJointDef();
    def.bodyIdA = bodyCompA->getBodyId();
    def.bodyIdB = bodyCompB->getBodyId();
    def.localAnchorA = anchorA;
    def.localAnchorB = anchorB;
    
    if (hertz > 0.0f) {
        def.linearHertz = hertz;
        def.angularHertz = hertz;
        def.linearDampingRatio = dampingRatio;
        def.angularDampingRatio = dampingRatio;
    }
    
    jointId = b2CreateWeldJoint(engine->getPhysicsWorld(), &def);
    connectedBody = bodyB;
}

void JointComponent::createWheelJoint(Object* bodyB, const b2Vec2& anchorA, const b2Vec2& anchorB, const b2Vec2& axis,
                                     float hertz, float dampingRatio,
                                     bool enableMotor, float motorSpeed, float maxMotorTorque) {
    if (jointBroken || B2_IS_NON_NULL(jointId)) {
        std::cerr << "Warning: Joint already exists or is broken" << std::endl;
        return;
    }
    
    Engine* engine = Object::getEngine();
    if (!engine) return;
    
    BodyComponent* bodyCompA = parent().getComponent<BodyComponent>();
    BodyComponent* bodyCompB = bodyB ? bodyB->getComponent<BodyComponent>() : nullptr;
    
    if (!bodyCompA || !bodyCompB) {
        std::cerr << "Error: Both objects must have BodyComponent" << std::endl;
        return;
    }
    
    b2WheelJointDef def = b2DefaultWheelJointDef();
    def.bodyIdA = bodyCompA->getBodyId();
    def.bodyIdB = bodyCompB->getBodyId();
    def.localAnchorA = anchorA;
    def.localAnchorB = anchorB;
    def.localAxisA = axis;
    
    if (hertz > 0.0f) {
        def.enableSpring = true;
        def.hertz = hertz;
        def.dampingRatio = dampingRatio;
    }
    
    if (enableMotor) {
        def.enableMotor = true;
        def.motorSpeed = Engine::degreesToRadians(motorSpeed);
        def.maxMotorTorque = maxMotorTorque;
    }
    
    jointId = b2CreateWheelJoint(engine->getPhysicsWorld(), &def);
    connectedBody = bodyB;
}

void JointComponent::createMotorJoint(Object* bodyB, const b2Vec2& linearOffset,
                                     float angularOffset, float maxForce, float maxTorque) {
    // Destroy any existing joint first
    if (B2_IS_NON_NULL(jointId)) {
        destroyJoint();
    }
    
    // Reset broken flag to allow creation
    jointBroken = false;
    
    Engine* engine = Object::getEngine();
    if (!engine) return;
    
    BodyComponent* bodyCompA = parent().getComponent<BodyComponent>();
    BodyComponent* bodyCompB = bodyB ? bodyB->getComponent<BodyComponent>() : nullptr;
    
    if (!bodyCompA || !bodyCompB) {
        std::cerr << "Error: Both objects must have BodyComponent" << std::endl;
        return;
    }
    
    b2MotorJointDef def = b2DefaultMotorJointDef();
    def.bodyIdA = bodyCompA->getBodyId();
    def.bodyIdB = bodyCompB->getBodyId();
    def.linearOffset = linearOffset;
    def.angularOffset = Engine::degreesToRadians(angularOffset);
    def.maxForce = maxForce;
    def.maxTorque = maxTorque;
    
    jointId = b2CreateMotorJoint(engine->getPhysicsWorld(), &def);
    connectedBody = bodyB;
}

void JointComponent::createMouseJoint(Object* bodyB, const b2Vec2& target,
                                     float hertz, float dampingRatio, float maxForce) {
    if (jointBroken || B2_IS_NON_NULL(jointId)) {
        std::cerr << "Warning: Joint already exists or is broken" << std::endl;
        return;
    }
    
    Engine* engine = Object::getEngine();
    if (!engine) return;
    
    BodyComponent* bodyCompA = parent().getComponent<BodyComponent>();
    BodyComponent* bodyCompB = bodyB ? bodyB->getComponent<BodyComponent>() : nullptr;
    
    if (!bodyCompA || !bodyCompB) {
        std::cerr << "Error: Both objects must have BodyComponent" << std::endl;
        return;
    }
    
    b2MouseJointDef def = b2DefaultMouseJointDef();
    def.bodyIdA = bodyCompA->getBodyId();
    def.bodyIdB = bodyCompB->getBodyId();
    def.target = target;
    def.hertz = hertz;
    def.dampingRatio = dampingRatio;
    def.maxForce = maxForce;
    
    jointId = b2CreateMouseJoint(engine->getPhysicsWorld(), &def);
    connectedBody = bodyB;
}

void JointComponent::createFilterJoint(Object* bodyB) {
    if (jointBroken || B2_IS_NON_NULL(jointId)) {
        std::cerr << "Warning: Joint already exists or is broken" << std::endl;
        return;
    }
    
    Engine* engine = Object::getEngine();
    if (!engine) return;
    
    BodyComponent* bodyCompA = parent().getComponent<BodyComponent>();
    BodyComponent* bodyCompB = bodyB ? bodyB->getComponent<BodyComponent>() : nullptr;
    
    if (!bodyCompA || !bodyCompB) {
        std::cerr << "Error: Both objects must have BodyComponent" << std::endl;
        return;
    }
    
    b2FilterJointDef def = b2DefaultFilterJointDef();
    def.bodyIdA = bodyCompA->getBodyId();
    def.bodyIdB = bodyCompB->getBodyId();
    
    jointId = b2CreateFilterJoint(engine->getPhysicsWorld(), &def);
    connectedBody = bodyB;
}

// Query methods
b2JointType JointComponent::getJointType() const {
    if (B2_IS_NULL(jointId)) return b2_distanceJoint; // default
    return b2Joint_GetType(jointId);
}

b2Vec2 JointComponent::getConstraintForce() const {
    if (B2_IS_NULL(jointId)) return {0.0f, 0.0f};
    return b2Joint_GetConstraintForce(jointId);
}

float JointComponent::getConstraintTorque() const {
    if (B2_IS_NULL(jointId)) return 0.0f;
    return b2Joint_GetConstraintTorque(jointId);
}

float JointComponent::getLinearSeparation() const {
    if (B2_IS_NULL(jointId)) return 0.0f;
    return b2Joint_GetLinearSeparation(jointId) * Engine::METERS_TO_PIXELS;
}

float JointComponent::getAngularSeparation() const {
    if (B2_IS_NULL(jointId)) return 0.0f;
    return Engine::radiansToDegrees(b2Joint_GetAngularSeparation(jointId));
}

// Helper methods
std::string JointComponent::jointTypeToString(b2JointType type) const {
    switch (type) {
        case b2_distanceJoint: return "distance";
        case b2_filterJoint: return "filter";
        case b2_motorJoint: return "motor";
        case b2_mouseJoint: return "mouse";
        case b2_prismaticJoint: return "prismatic";
        case b2_revoluteJoint: return "revolute";
        case b2_weldJoint: return "weld";
        case b2_wheelJoint: return "wheel";
        default: return "unknown";
    }
}

b2Vec2 JointComponent::pixelsToMeters(const b2Vec2& pixels) const {
    return {pixels.x * Engine::PIXELS_TO_METERS, pixels.y * Engine::PIXELS_TO_METERS};
}

b2Vec2 JointComponent::metersToPixels(const b2Vec2& meters) const {
    return {meters.x * Engine::METERS_TO_PIXELS, meters.y * Engine::METERS_TO_PIXELS};
}

