#ifndef PLAYER_H
#define PLAYER_H

#include "Object.h"
#include <nlohmann/json.hpp>

class Player : public Object {
    public:
        Player();
        Player(const nlohmann::json& data);
        ~Player()=default;
        void update() override;
        void render(SDL_Renderer* renderer) override;

        void setVelocity(Vector2 velocity);
        Vector2 getVelocity();
    private:
        Vector2 velocity;
        int speed;
};


#endif // PLAYER_H