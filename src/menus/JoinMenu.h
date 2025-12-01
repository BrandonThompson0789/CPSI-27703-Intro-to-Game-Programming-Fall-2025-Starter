#ifndef JOIN_MENU_H
#define JOIN_MENU_H

#include "Menu.h"
#include <string>
#include <future>
#include <memory>
#include <atomic>
#include <SDL.h>
#include <SDL_ttf.h>

class Engine;

class JoinMenu : public Menu {
public:
    JoinMenu(MenuManager* manager);
    virtual ~JoinMenu();
    
    void update(float deltaTime) override;
    void onOpen() override;
    void handleNavigation(float upDown, float leftRight) override;
    void handleConfirm() override;
    void handleCancel() override;
    void handleMouse(int mouseX, int mouseY, bool mousePressed) override;
    
    // Custom rendering
    bool render() override;
    
    // Handle text input
    void handleTextInput(const std::string& text) override;
    void handleBackspace() override;
    
private:
    void setupMenuItems();
    void onJoin();
    void onPaste();
    void onBack();
    void updateJoinButtonState();
    std::future<bool> launchJoinTask(Engine* engine, std::string roomCode, const std::shared_ptr<std::atomic<bool>>& cancelToken);
    void resetJoinFuture();
    void updateConnectingStatus(float deltaTime);
    
    std::string roomCodeInput;
    bool joinButtonEnabled;
    int selectedButtonIndex;  // 0 = Join, 1 = Back
    int hoveredButtonIndex;
    bool mouseMode;
    
    // Rendering resources
    TTF_Font* font;
    TTF_Font* buttonFont;
    SDL_Texture* roomCodeTexture;
    SDL_Texture* joinButtonTexture;
    SDL_Texture* pasteButtonTexture;
    SDL_Texture* backButtonTexture;
    SDL_Texture* placeholderTexture;
    int roomCodeTextureWidth;
    int roomCodeTextureHeight;
    
    void loadResources();
    void unloadResources();
    void updateRoomCodeTexture();
    
    enum class State {
        INPUT,
        CONNECTING,
        FAILED
    };
    
    State state;
    float connectionTimeout;
    bool joinAttemptInProgress;
    bool connectionInitiated;
    bool cancelRequested;
    float spinnerTimer;
    int spinnerIndex;
    std::string connectingStatus;
    std::string failureMessage;
    std::future<bool> joinFuture;
    std::shared_ptr<std::atomic<bool>> joinCancelToken;
    static constexpr float CONNECTION_TIMEOUT_SECONDS = 5.0f;
    static constexpr int ROOM_CODE_LENGTH = 6;
    static constexpr float SPINNER_INTERVAL = 0.2f;
};

#endif // JOIN_MENU_H

