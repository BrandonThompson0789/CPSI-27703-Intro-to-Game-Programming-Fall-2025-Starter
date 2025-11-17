#ifndef MULTIPLAYER_MENU_H
#define MULTIPLAYER_MENU_H

#include "Menu.h"

class MultiplayerMenu : public Menu {
public:
    MultiplayerMenu(MenuManager* manager);
    virtual ~MultiplayerMenu() = default;
    
    void handleCancel() override;
    
private:
    void setupMenuItems();
    void onHost();
    void onJoin();
    void onGoBack();
};

#endif // MULTIPLAYER_MENU_H

