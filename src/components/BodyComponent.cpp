#include "BodyComponent.h"
#include "ComponentLibrary.h"
#include "../Engine.h"
#include "../Object.h"
#include <iostream>

BodyComponent::BodyComponent(Object& parent) : Component(parent), bodyId(b2_nullBodyId) {
    createDefaultBody();
}

BodyComponent::BodyComponent(Object& parent, float drag) : Component(parent), bodyId(b2_nullBodyId) {
    createDefaultBody();
    if (B2_IS_NON_NULL(bodyId)) {
        b2Body_SetLinearDamping(bodyId, drag);
    }
}

BodyComponent::BodyComponent(Object& parent, const nlohmann::json& data) : Component(parent), bodyId(b2_nullBodyId) {
    createBodyFromJson(data);
}

BodyComponent::~BodyComponent() {
    // Destroy the Box2D body when component is destroyed (v3.x API)
    if (B2_IS_NON_NULL(bodyId)) {
        b2DestroyBody(bodyId);
        bodyId = b2_nullBodyId;
    }
}

nlohmann::json BodyComponent::toJson() const {
    nlohmann::json j;
    j["type"] = getTypeName();
    
    if (B2_IS_NULL(bodyId)) return j;
    
    // Get position from Box2D body (convert back to pixels)
    b2Vec2 pos = b2Body_GetPosition(bodyId);
    b2Rot rot = b2Body_GetRotation(bodyId);
    float angle = Engine::radiansToDegrees(b2Rot_GetAngle(rot));
    
    j["posX"] = pos.x * Engine::METERS_TO_PIXELS;
    j["posY"] = pos.y * Engine::METERS_TO_PIXELS;
    j["angle"] = angle;
    
    // Body properties
    j["bodyType"] = bodyTypeToString(b2Body_GetType(bodyId));
    j["fixedRotation"] = b2Body_IsFixedRotation(bodyId);
    j["bullet"] = b2Body_IsBullet(bodyId);
    j["linearDamping"] = b2Body_GetLinearDamping(bodyId);
    j["angularDamping"] = b2Body_GetAngularDamping(bodyId);
    j["gravityScale"] = b2Body_GetGravityScale(bodyId);
    
    // Velocity (for serialization)
    b2Vec2 vel = b2Body_GetLinearVelocity(bodyId);
    j["velX"] = vel.x * Engine::METERS_TO_PIXELS;
    j["velY"] = vel.y * Engine::METERS_TO_PIXELS;
    j["velAngle"] = Engine::radiansToDegrees(b2Body_GetAngularVelocity(bodyId));
    
    return j;
}

// Register this component type with the library
static ComponentRegistrar<BodyComponent> registrar("BodyComponent");

void BodyComponent::setPosition(float x, float y, float angle) {
    if (B2_IS_NULL(bodyId)) return;
    
    // Convert pixels to meters
    b2Vec2 position = {x * Engine::PIXELS_TO_METERS, y * Engine::PIXELS_TO_METERS};
    b2Rot rotation = b2MakeRot(Engine::degreesToRadians(angle));
    b2Body_SetTransform(bodyId, position, rotation);
}

void BodyComponent::setVelocity(float x, float y, float angle) {
    if (B2_IS_NULL(bodyId)) return;
    
    // Convert pixels/second to meters/second
    b2Vec2 velocity = {x * Engine::PIXELS_TO_METERS, y * Engine::PIXELS_TO_METERS};
    b2Body_SetLinearVelocity(bodyId, velocity);
    b2Body_SetAngularVelocity(bodyId, Engine::degreesToRadians(angle));
}

void BodyComponent::modVelocity(float x, float y, float angle) {
    if (B2_IS_NULL(bodyId)) return;
    
    // Get current velocity and add to it
    b2Vec2 currentVel = b2Body_GetLinearVelocity(bodyId);
    b2Vec2 deltaVel = {x * Engine::PIXELS_TO_METERS, y * Engine::PIXELS_TO_METERS};
    
    b2Vec2 newVel = b2Add(currentVel, deltaVel);
    b2Body_SetLinearVelocity(bodyId, newVel);
    b2Body_SetAngularVelocity(bodyId, b2Body_GetAngularVelocity(bodyId) + Engine::degreesToRadians(angle));
}

std::tuple<float, float, float> BodyComponent::getPosition() {
    if (B2_IS_NULL(bodyId)) return {0.0f, 0.0f, 0.0f};
    
    // Convert meters to pixels
    b2Vec2 pos = b2Body_GetPosition(bodyId);
    b2Rot rot = b2Body_GetRotation(bodyId);
    float angle = Engine::radiansToDegrees(b2Rot_GetAngle(rot));
    
    return std::make_tuple(
        pos.x * Engine::METERS_TO_PIXELS, 
        pos.y * Engine::METERS_TO_PIXELS, 
        angle
    );
}

std::tuple<float, float, float> BodyComponent::getVelocity() {
    if (B2_IS_NULL(bodyId)) return {0.0f, 0.0f, 0.0f};
    
    // Convert meters/second to pixels/second
    b2Vec2 vel = b2Body_GetLinearVelocity(bodyId);
    return std::make_tuple(
        vel.x * Engine::METERS_TO_PIXELS, 
        vel.y * Engine::METERS_TO_PIXELS, 
        Engine::radiansToDegrees(b2Body_GetAngularVelocity(bodyId))
    );
}

void BodyComponent::update(float deltaTime) {
    // Box2D handles physics simulation automatically in Engine::update()
    // This method can be used for custom per-body logic if needed
}

void BodyComponent::draw() {
    // BodyComponent doesn't render anything itself
    // Rendering is typically handled by other components (e.g., SpriteComponent)
}

// Helper method to create body from JSON
void BodyComponent::createBodyFromJson(const nlohmann::json& data) {
    Engine* engine = Object::getEngine();
    if (!engine) {
        std::cerr << "Error: Engine not available!" << std::endl;
        return;
    }
    
    b2WorldId worldId = engine->getPhysicsWorld();
    if (B2_IS_NULL(worldId)) {
        std::cerr << "Error: Physics world not available!" << std::endl;
        return;
    }
    
    // Parse position (in pixels, convert to meters)
    float posX = data.value("posX", 0.0f);
    float posY = data.value("posY", 0.0f);
    float angle = data.value("angle", 0.0f);
    
    // Check if this is legacy JSON (no bodyType field)
    bool isLegacy = !data.contains("bodyType") && !data.contains("fixture");
    
    if (isLegacy) {
        // Legacy mode: create simple dynamic body with default settings
        createDefaultBody(posX, posY, angle);
        
        // Apply legacy drag if specified
        if (data.contains("drag")) {
            b2Body_SetLinearDamping(bodyId, data["drag"].get<float>());
        }
        
        // Apply initial velocity if specified (legacy support)
        if (data.contains("velX") || data.contains("velY")) {
            float velX = data.value("velX", 0.0f);
            float velY = data.value("velY", 0.0f);
            float velAngle = data.value("velAngle", 0.0f);
            setVelocity(velX, velY, velAngle);
        }
    } else {
        // New mode: parse Box2D properties
        std::string bodyTypeStr = data.value("bodyType", "dynamic");
        b2BodyType bodyType = parseBodyType(bodyTypeStr);
        
        bool fixedRotation = data.value("fixedRotation", false);
        bool bullet = data.value("bullet", false);
        float linearDamping = data.value("linearDamping", 0.5f);
        float angularDamping = data.value("angularDamping", 0.3f);
        float gravityScale = data.value("gravityScale", 1.0f);
        
        // Create Box2D body definition (v3.x API)
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = bodyType;
        bodyDef.position = {posX * Engine::PIXELS_TO_METERS, posY * Engine::PIXELS_TO_METERS};
        bodyDef.rotation = b2MakeRot(Engine::degreesToRadians(angle));
        bodyDef.fixedRotation = fixedRotation;
        bodyDef.isBullet = bullet;
        bodyDef.linearDamping = linearDamping;
        bodyDef.angularDamping = angularDamping;
        bodyDef.gravityScale = gravityScale;
        
        // Create body in the physics world
        bodyId = b2CreateBody(worldId, &bodyDef);
        
        // Store pointer to parent object in user data
        b2Body_SetUserData(bodyId, &parent());
        
        // Create fixture if specified
        if (data.contains("fixture")) {
            createFixtureFromJson(data["fixture"]);
        } else {
            // Create default fixture (32x32 box)
            b2Polygon boxShape = b2MakeBox(16.0f * Engine::PIXELS_TO_METERS, 16.0f * Engine::PIXELS_TO_METERS);
            
            b2ShapeDef shapeDef = b2DefaultShapeDef();
            shapeDef.density = 1.0f;
            // Note: Box2D v3.x uses different friction/restitution API
            // These are set per-shape after creation if needed
            
            b2CreatePolygonShape(bodyId, &shapeDef, &boxShape);
        }
    }
}

// Helper method to create a default body
void BodyComponent::createDefaultBody(float posX, float posY, float angle) {
    Engine* engine = Object::getEngine();
    if (!engine) {
        std::cerr << "Error: Engine not available!" << std::endl;
        return;
    }
    
    b2WorldId worldId = engine->getPhysicsWorld();
    if (B2_IS_NULL(worldId)) {
        std::cerr << "Error: Physics world not available!" << std::endl;
        return;
    }
    
    // Create a simple dynamic body (v3.x API)
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = {posX * Engine::PIXELS_TO_METERS, posY * Engine::PIXELS_TO_METERS};
    bodyDef.rotation = b2MakeRot(Engine::degreesToRadians(angle));
    bodyDef.linearDamping = 0.5f;
    bodyDef.angularDamping = 0.3f;
    
    bodyId = b2CreateBody(worldId, &bodyDef);
    b2Body_SetUserData(bodyId, &parent());
    
    // Create default 32x32 box fixture
    b2Polygon boxShape = b2MakeBox(16.0f * Engine::PIXELS_TO_METERS, 16.0f * Engine::PIXELS_TO_METERS);
    
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 1.0f;
    // Note: Box2D v3.x uses different friction/restitution API
    // These are set per-shape after creation if needed
    
    b2CreatePolygonShape(bodyId, &shapeDef, &boxShape);
}

// Helper method to create fixture from JSON
void BodyComponent::createFixtureFromJson(const nlohmann::json& fixtureData) {
    if (B2_IS_NULL(bodyId)) return;
    
    std::string shapeType = fixtureData.value("shape", "box");
    float density = fixtureData.value("density", 1.0f);
    float friction = fixtureData.value("friction", 0.3f);
    float restitution = fixtureData.value("restitution", 0.0f);
    bool isSensor = fixtureData.value("isSensor", false);
    
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = density;
    // Note: In Box2D v3.x, friction and restitution are set differently
    // For now, we'll just use density and isSensor
    shapeDef.isSensor = isSensor;
    
    // Create shape based on type (v3.x API)
    if (shapeType == "box") {
        float width = fixtureData.value("width", 32.0f);
        float height = fixtureData.value("height", 32.0f);
        
        b2Polygon boxShape = b2MakeBox(
            (width / 2.0f) * Engine::PIXELS_TO_METERS, 
            (height / 2.0f) * Engine::PIXELS_TO_METERS
        );
        
        b2CreatePolygonShape(bodyId, &shapeDef, &boxShape);
    } 
    else if (shapeType == "circle") {
        float radius = fixtureData.value("radius", 16.0f);
        
        b2Circle circleShape;
        circleShape.center = {0.0f, 0.0f};
        circleShape.radius = radius * Engine::PIXELS_TO_METERS;
        
        b2CreateCircleShape(bodyId, &shapeDef, &circleShape);
    }
    else {
        std::cerr << "Warning: Unknown shape type '" << shapeType << "', using default box" << std::endl;
        
        b2Polygon boxShape = b2MakeBox(16.0f * Engine::PIXELS_TO_METERS, 16.0f * Engine::PIXELS_TO_METERS);
        b2CreatePolygonShape(bodyId, &shapeDef, &boxShape);
    }
}

// Parse body type from string
b2BodyType BodyComponent::parseBodyType(const std::string& typeStr) {
    if (typeStr == "static") return b2_staticBody;
    if (typeStr == "kinematic") return b2_kinematicBody;
    return b2_dynamicBody; // default
}

// Convert body type to string
std::string BodyComponent::bodyTypeToString(b2BodyType type) const {
    switch (type) {
        case b2_staticBody: return "static";
        case b2_kinematicBody: return "kinematic";
        case b2_dynamicBody: return "dynamic";
        default: return "dynamic";
    }
}

// Get fixture size in pixels
std::tuple<float, float> BodyComponent::getFixtureSize() const {
    if (B2_IS_NULL(bodyId)) {
        return {0.0f, 0.0f};
    }
    
    // Get the first shape from the body
    int shapeCount = b2Body_GetShapeCount(bodyId);
    if (shapeCount == 0) {
        return {0.0f, 0.0f};
    }
    
    // Box2D v3 requires a buffer array
    b2ShapeId shapeId;
    b2Body_GetShapes(bodyId, &shapeId, 1);
    
    b2ShapeType shapeType = b2Shape_GetType(shapeId);
    
    if (shapeType == b2_polygonShape) {
        // Get polygon and calculate bounding box
        b2Polygon polygon = b2Shape_GetPolygon(shapeId);
        
        // Find min/max extents
        float minX = polygon.vertices[0].x;
        float maxX = polygon.vertices[0].x;
        float minY = polygon.vertices[0].y;
        float maxY = polygon.vertices[0].y;
        
        for (int i = 1; i < polygon.count; ++i) {
            if (polygon.vertices[i].x < minX) minX = polygon.vertices[i].x;
            if (polygon.vertices[i].x > maxX) maxX = polygon.vertices[i].x;
            if (polygon.vertices[i].y < minY) minY = polygon.vertices[i].y;
            if (polygon.vertices[i].y > maxY) maxY = polygon.vertices[i].y;
        }
        
        float width = (maxX - minX) * Engine::METERS_TO_PIXELS;
        float height = (maxY - minY) * Engine::METERS_TO_PIXELS;
        
        return {width, height};
    }
    else if (shapeType == b2_circleShape) {
        // Get circle radius and calculate diameter
        b2Circle circle = b2Shape_GetCircle(shapeId);
        float diameter = circle.radius * 2.0f * Engine::METERS_TO_PIXELS;
        
        return {diameter, diameter};
    }
    
    // Unknown shape type
    return {0.0f, 0.0f};
}
