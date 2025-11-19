#ifndef HOST_MENU_H
#define HOST_MENU_H

#include "Menu.h"
#include <string>

class HostMenu : public Menu {
public:
    HostMenu(MenuManager* manager);
    virtual ~HostMenu() = default;
    
    void onOpen() override;
    void update(float deltaTime) override;
    void handleCancel() override;
    
private:
    void setupMenuItems();
    void updateMenuItems();
    void onBack();
    void startHostingAttempt();
    
    enum class State {
        CONNECTING,
        SUCCESS,
        FAILED
    };
    
    State state;
    std::string roomCode;
    float connectionTimeout;
    bool hostingAttemptStarted;
    bool levelSelectOpened;
    static constexpr float CONNECTION_TIMEOUT_SECONDS = 5.0f;
};

#endif // HOST_MENU_H

