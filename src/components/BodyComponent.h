#pragma once
#include "Component.h"
#include <tuple>

class BodyComponent : Component {

    public:
        BodyComponent(Object& parent);
        void setPosition(float x, float y, float angle);
        void setVelocity(float x, float y, float angle);
        std::tuple<float,float,float> getPosition();
        std::tuple<float,float,float> getVelocity();

    private:
        float posX{};
        float posY{};
        float angle{};
        float velX{};
        float velY{};
        float velAngle{};
};