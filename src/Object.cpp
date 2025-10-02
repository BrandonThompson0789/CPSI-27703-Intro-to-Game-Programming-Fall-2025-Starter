#include "Object.h"

void Object::setPosition(Vector2 pos) {
    position = pos;
}

void Object::setSize(Vector2 s) {
    size = s;
}

void Object::setAngle(float a) {
    angle = a;
}

Vector2 Object::getPosition() {
    return position;
}

Vector2 Object::getSize() {
    return size;
}

float Object::getAngle() {
    return angle;
}