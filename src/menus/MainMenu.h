#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include "Menu.h"

class MainMenu : public Menu {
public:
    MainMenu(MenuManager* manager);
    virtual ~MainMenu() = default;
    
    void onOpen() override;
    void handleCancel() override;
    
private:
    void setupMenuItems();
    void onContinue();
    void onPlay();
    void onMultiplayer();
    void onSettings();
    void onQuit();
};

#endif // MAIN_MENU_H

