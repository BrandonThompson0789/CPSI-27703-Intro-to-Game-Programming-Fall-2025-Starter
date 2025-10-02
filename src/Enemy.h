#ifndef ENEMY_H
#define ENEMY_H

#include "Object.h"
#include <nlohmann/json.hpp>

class Enemy : public Object {
    public:
        Enemy();
        Enemy(const nlohmann::json& data);
        ~Enemy()=default;
        void update() override;
        void render(SDL_Renderer* renderer) override;

        void setHealth(int health);
        int getHealth();
        void setDamage(int damage);
        int getDamage();
    private:
        int health;
        int damage;
};


#endif // ENEMY_H
