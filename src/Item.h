#ifndef ITEM_H
#define ITEM_H

#include "Object.h"
#include <string>
#include <nlohmann/json.hpp>

class Item : public Object {
    public:
        Item();
        Item(const nlohmann::json& data);
        ~Item()=default;
        void update() override;
        void render(SDL_Renderer* renderer) override;

        void setItemType(const std::string& type);
        std::string getItemType();
        void setValue(int value);
        int getValue();
    private:
        std::string itemType;
        int value;
};


#endif // ITEM_H
