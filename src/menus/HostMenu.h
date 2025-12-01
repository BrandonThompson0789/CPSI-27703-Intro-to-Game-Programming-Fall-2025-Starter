#ifndef HOST_MENU_H
#define HOST_MENU_H

#include "Menu.h"
#include <string>
#include <future>
#include <memory>
#include <atomic>

class Engine;

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
    void updateConnectingStatus(float deltaTime);
    std::future<std::string> launchHostingTask(Engine* engine, const std::shared_ptr<std::atomic<bool>>& cancelToken);
    void resetHostingFuture();
    
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
    bool cancelRequested;
    float spinnerTimer;
    int spinnerIndex;
    std::string connectingStatus;
    std::string failureMessage;
    std::future<std::string> hostingFuture;
    std::shared_ptr<std::atomic<bool>> hostingCancelToken;
    static constexpr float CONNECTION_TIMEOUT_SECONDS = 5.0f;
    static constexpr float SPINNER_INTERVAL = 0.2f;
};

#endif // HOST_MENU_H

