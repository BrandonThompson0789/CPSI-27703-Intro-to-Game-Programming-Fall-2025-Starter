#ifndef HOST_SESSION_ENDED_MENU_H
#define HOST_SESSION_ENDED_MENU_H

#include "Menu.h"
#include <string>

class HostSessionEndedMenu : public Menu {
public:
    HostSessionEndedMenu(MenuManager* manager);
    virtual ~HostSessionEndedMenu() = default;
    
    void handleCancel() override;
    
private:
    void setupMenuItems();
    void onReturnToMainMenu();
};

#endif // HOST_SESSION_ENDED_MENU_H

