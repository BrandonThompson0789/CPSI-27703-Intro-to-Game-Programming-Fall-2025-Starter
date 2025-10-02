#include "Enemy.h"

Enemy::Enemy() {
    position = {200, 200};
    size = {24, 24};
    health = 100;
    damage = 10;
}

Enemy::Enemy(const nlohmann::json& data) {
    // Initialize from JSON data
    if (data.contains("position")) {
        position.x = data["position"]["x"].get<float>();
        position.y = data["position"]["y"].get<float>();
    } else {
        position = {200, 200};
    }
    
    if (data.contains("size")) {
        size.x = data["size"]["x"].get<float>();
        size.y = data["size"]["y"].get<float>();
    } else {
        size = {24, 24};
    }

    if (data.contains("angle"))
        angle = data["angle"].get<float>();
    else
        angle = 0;
    
    if (data.contains("health")) {
        health = data["health"].get<int>();
    } else {
        health = 100;
    }
    
    if (data.contains("damage")) {
        damage = data["damage"].get<int>();
    } else {
        damage = 10;
    }
}

void Enemy::update() {
    // Basic AI - move left and right
    position.x += 1.0f;
    if (position.x > 800) {
        position.x = 0-size.x;
    }
}

void Enemy::render(SDL_Renderer* renderer) {
    // Render enemy as a red rectangle
    SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255); // Red color
    SDL_Rect enemyRect = {
        static_cast<int>(position.x), 
        static_cast<int>(position.y), 
        static_cast<int>(size.x), 
        static_cast<int>(size.y)
    };
    SDL_RenderFillRect(renderer, &enemyRect);
}

void Enemy::setHealth(int h) {
    health = h;
}

int Enemy::getHealth() {
    return health;
}

void Enemy::setDamage(int d) {
    damage = d;
}

int Enemy::getDamage() {
    return damage;
}

