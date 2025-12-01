#include "HostMenu.h"
#include "MenuManager.h"
#include "../Engine.h"
#include <iostream>
#include <thread>
#include <chrono>

HostMenu::HostMenu(MenuManager* manager)
    : Menu(manager),
      state(State::CONNECTING),
      roomCode(""),
      connectionTimeout(0.0f),
      hostingAttemptStarted(false),
      levelSelectOpened(false),
      cancelRequested(false),
      spinnerTimer(0.0f),
      spinnerIndex(0),
      connectingStatus("Connecting to server manager |"),
      failureMessage(""),
      hostingCancelToken(nullptr) {
    setTitle("Hosting Game");
    setupMenuItems();
}

void HostMenu::setupMenuItems() {
    clearItems();
    updateMenuItems();
}

void HostMenu::updateMenuItems() {
    clearItems();
    
    if (state == State::CONNECTING) {
        std::string status = connectingStatus.empty() ? "Connecting to server..." : connectingStatus;
        addItem(status, [this]() {});
    } else if (state == State::SUCCESS && !roomCode.empty()) {
        addItem("Room Code: " + roomCode, [this]() {});
        addItem("Opening level select...", [this]() {});
    } else if (state == State::FAILED) {
        std::string message = failureMessage.empty() ? "Failed to connect to server manager" : failureMessage;
        addItem(message, [this]() {});
        addItem("Returning to menu...", [this]() {});
    } else {
        addItem("Connecting to server...", [this]() {});
    }
}

void HostMenu::onOpen() {
    Menu::onOpen();
    
    // Reset state when menu opens
    state = State::CONNECTING;
    roomCode.clear();
    failureMessage.clear();
    connectionTimeout = 0.0f;
    levelSelectOpened = false;
    cancelRequested = false;
    spinnerTimer = 0.0f;
    spinnerIndex = 0;
    connectingStatus = "Connecting to server manager |";
    resetHostingFuture();
    
    // Start hosting attempt asynchronously
    startHostingAttempt();
}

void HostMenu::startHostingAttempt() {
    if (hostingAttemptStarted) {
        return;
    }
    
    hostingAttemptStarted = true;
    failureMessage.clear();
    connectionTimeout = 0.0f;
    if (!menuManager || !menuManager->getEngine()) {
        state = State::FAILED;
        failureMessage = "Engine unavailable";
        connectionTimeout = CONNECTION_TIMEOUT_SECONDS;
        hostingAttemptStarted = false;
        return;
    }
    
    Engine* engine = menuManager->getEngine();
    
    hostingCancelToken = std::make_shared<std::atomic<bool>>(false);
    hostingFuture = launchHostingTask(engine, hostingCancelToken);
    updateMenuItems();
}

std::future<std::string> HostMenu::launchHostingTask(Engine* engine, const std::shared_ptr<std::atomic<bool>>& cancelToken) {
    auto task = std::make_shared<std::packaged_task<std::string()>>([engine, cancelToken]() {
        std::string code = engine ? engine->startHosting() : "";
        if (cancelToken && cancelToken->load()) {
            if (engine) {
                engine->stopHosting();
            }
            return std::string();
        }
        return code;
    });
    
    std::future<std::string> future = task->get_future();
    std::thread([task]() {
        (*task)();
    }).detach();
    return future;
}

void HostMenu::resetHostingFuture() {
    hostingFuture = std::future<std::string>();
    hostingAttemptStarted = false;
    hostingCancelToken.reset();
}

void HostMenu::updateConnectingStatus(float deltaTime) {
    if (!hostingAttemptStarted) {
        return;
    }
    
    spinnerTimer += deltaTime;
    if (spinnerTimer >= SPINNER_INTERVAL) {
        spinnerTimer = 0.0f;
        spinnerIndex = (spinnerIndex + 1) % 4;
        static constexpr char SPINNER_FRAMES[] = {'|', '/', '-', '\\'};
        connectingStatus = "Connecting to server manager ";
        connectingStatus.push_back(SPINNER_FRAMES[spinnerIndex]);
        updateMenuItems();
    }
}

void HostMenu::update(float deltaTime) {
    Menu::update(deltaTime);
    
    if (state == State::FAILED && connectionTimeout > 0.0f) {
        connectionTimeout -= deltaTime;
        if (connectionTimeout <= 0.0f) {
            // Return to multiplayer menu
            if (menuManager) {
                menuManager->closeMenu();
            }
        }
    } else if (state == State::SUCCESS && !roomCode.empty() && !levelSelectOpened) {
        // Hosting succeeded - open level select menu
        // Only do this once
        levelSelectOpened = true;
        if (menuManager) {
            menuManager->openMenu("level_select");
        }
    }
    
    if (state == State::CONNECTING) {
        updateConnectingStatus(deltaTime);
        if (hostingAttemptStarted && hostingFuture.valid()) {
            auto status = hostingFuture.wait_for(std::chrono::milliseconds(0));
            if (status == std::future_status::ready) {
                std::string code = hostingFuture.get();
                resetHostingFuture();
                
                Engine* engine = menuManager ? menuManager->getEngine() : nullptr;
                if (!cancelRequested && !code.empty()) {
                    state = State::SUCCESS;
                    roomCode = code;
                    updateMenuItems();
                } else {
                    std::string message = cancelRequested ? "Hosting canceled" : "Failed to connect to server manager";
                    failureMessage = message;
                    state = State::FAILED;
                    connectionTimeout = CONNECTION_TIMEOUT_SECONDS;
                    updateMenuItems();
                    if (engine) {
                        engine->displayMessage(message);
                        engine->stopHosting();
                    }
                }
            }
        }
    }
}

void HostMenu::handleCancel() {
    cancelRequested = true;
    if (hostingCancelToken) {
        hostingCancelToken->store(true);
    }
    onBack();
}

void HostMenu::onBack() {
    cancelRequested = true;
    if (hostingCancelToken) {
        hostingCancelToken->store(true);
    }
    
    // Stop hosting if it was started
    if (menuManager && menuManager->getEngine()) {
        Engine* engine = menuManager->getEngine();
        if (engine->isHosting()) {
            engine->stopHosting();
        }
    }
    
    resetHostingFuture();
    
    if (menuManager) {
        menuManager->closeMenu();
    }
}

