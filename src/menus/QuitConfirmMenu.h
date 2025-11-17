#ifndef QUIT_CONFIRM_MENU_H
#define QUIT_CONFIRM_MENU_H

#include "Menu.h"

class QuitConfirmMenu : public Menu {
public:
    QuitConfirmMenu(MenuManager* manager);
    virtual ~QuitConfirmMenu() = default;
    
    void handleCancel() override;
    
private:
    void setupMenuItems();
    void onYes();
    void onNo();
};

#endif // QUIT_CONFIRM_MENU_H

