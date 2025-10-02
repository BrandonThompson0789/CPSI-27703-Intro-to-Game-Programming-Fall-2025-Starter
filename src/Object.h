#ifndef OBJECT_H
#define OBJECT_H

#include "Common.h"
#include <SDL.h>

class Object {
    public:
        Object()=default;
        virtual ~Object()=default;
        virtual void update()=0;
        virtual void render(SDL_Renderer* renderer)=0;
        virtual void setPosition(Vector2 position);
        virtual void setSize(Vector2 size);
        virtual void setAngle(float angle);
        virtual Vector2 getPosition();
        virtual Vector2 getSize();
        virtual float getAngle();
    protected:
        Vector2 position;
        Vector2 size;
        float angle;
};

#endif // OBJECT_H