#include "Item.h"

Item::Item() {
    position = {300, 300};
    size = {16, 16};
    itemType = "coin";
    value = 10;
}

Item::Item(const nlohmann::json& data) {
    // Initialize from JSON data
    if (data.contains("position")) {
        position.x = data["position"]["x"].get<float>();
        position.y = data["position"]["y"].get<float>();
    } else {
        position = {300, 300};
    }
    
    if (data.contains("size")) {
        size.x = data["size"]["x"].get<float>();
        size.y = data["size"]["y"].get<float>();
    } else {
        size = {16, 16};
    }

    if (data.contains("angle"))
        angle = data["angle"].get<float>();
    else
        angle = 0;
    
    if (data.contains("type")) {
        itemType = data["type"].get<std::string>();
    } else {
        itemType = "coin";
    }
    
    if (data.contains("value")) {
        value = data["value"].get<int>();
    } else {
        value = 10;
    }
}

void Item::update() {
    // Items might have simple animations or effects
}

void Item::render(SDL_Renderer* renderer) {
    // Render item as a yellow rectangle
    SDL_SetRenderDrawColor(renderer, 255, 255, 50, 255); // Yellow color
    SDL_Rect itemRect = {
        static_cast<int>(position.x), 
        static_cast<int>(position.y), 
        static_cast<int>(size.x), 
        static_cast<int>(size.y)
    };
    SDL_RenderFillRect(renderer, &itemRect);
}

void Item::setItemType(const std::string& type) {
    itemType = type;
}

std::string Item::getItemType() {
    return itemType;
}

void Item::setValue(int v) {
    value = v;
}

int Item::getValue() {
    return value;
}

