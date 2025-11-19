#ifndef WAITING_FOR_HOST_MENU_H
#define WAITING_FOR_HOST_MENU_H

#include "Menu.h"
#include <string>

class WaitingForHostMenu : public Menu {
public:
    WaitingForHostMenu(MenuManager* manager);
    virtual ~WaitingForHostMenu() = default;
    
    void update(float deltaTime) override;
    void handleCancel() override;
    
private:
    void setupMenuItems();
    void onBack();
    
    void checkHostStatus();
};

#endif // WAITING_FOR_HOST_MENU_H

