#ifndef PAUSE_MENU_H
#define PAUSE_MENU_H

#include "Menu.h"

class PauseMenu : public Menu {
public:
    PauseMenu(MenuManager* manager);
    virtual ~PauseMenu() = default;
    
    void handleCancel() override;
    
private:
    void setupMenuItems();
    void onResume();
    void onSave();
    void onSettings();
    void onQuitToMenu();
    void onQuitToDesktop();
};

#endif // PAUSE_MENU_H

