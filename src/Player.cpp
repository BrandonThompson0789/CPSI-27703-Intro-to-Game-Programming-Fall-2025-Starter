#include "Player.h"
#include "Engine.h"
#include <algorithm>

Player::Player() {
}

Player::Player(const nlohmann::json& data) {
    // Initialize from JSON data
    if (data.contains("position")) {
        position.x = data["position"]["x"].get<float>();
        position.y = data["position"]["y"].get<float>();
    } else {
        position = {100, 100};
    }
    
    if (data.contains("size")) {
        size.x = data["size"]["x"].get<float>();
        size.y = data["size"]["y"].get<float>();
    } else {
        size = {32, 32};
    }

    if (data.contains("angle"))
        angle = data["angle"].get<float>();
    else
        angle = 0;
    
    // Initialize velocity to zero (can be extended later if needed)
    velocity = {0, 0};
    speed = 10;
}

void Player::update() {
    //Process Input
    if (Engine::keyStates["left"]) 
        velocity.x = -speed;
    else if (Engine::keyStates["right"]) 
        velocity.x = speed;
    else
        velocity.x = 0;    
    
    if (Engine::keyStates["jump"])
        velocity.y = -10;
    
    //apply gravity
    velocity.y += 0.1;
    position.x += velocity.x;
    position.y += velocity.y;

    //clamp the position
    position.x = std::clamp(position.x, 0.0f, (float)(Engine::screenWidth-size.x));
    position.y = std::clamp(position.y, 0.0f, (float)(Engine::screenHeight-size.y));
    if(position.y==0)
        velocity.y=0;
}

void Player::render(SDL_Renderer* renderer) {
    // Render player as a blue rectangle
    SDL_SetRenderDrawColor(renderer, 0, 100, 255, 255); // Blue color
    SDL_Rect playerRect = {
        static_cast<int>(position.x), 
        static_cast<int>(position.y), 
        static_cast<int>(size.x), 
        static_cast<int>(size.y)
    };
    SDL_RenderFillRect(renderer, &playerRect);
}

void Player::setVelocity(Vector2 vel) {
    velocity = vel;
}

Vector2 Player::getVelocity() {
    return velocity;
}
