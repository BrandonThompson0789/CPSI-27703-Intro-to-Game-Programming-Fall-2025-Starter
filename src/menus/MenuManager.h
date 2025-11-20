#ifndef MENU_MANAGER_H
#define MENU_MANAGER_H

#include <SDL.h>
#include <memory>
#include <string>
#include <stack>

class Engine;
class Menu;

class MenuManager {
public:
    MenuManager(Engine* engine);
    ~MenuManager();
    
    // Initialize menu system
    bool init();
    
    // Cleanup
    void cleanup();
    
    // Update menus (call each frame)
    void update(float deltaTime);
    
    // Render menus (call each frame after game rendering)
    void render();
    
    // Handle SDL events (call from Engine::processEvents)
    void handleEvent(const SDL_Event& event);
    
    // Menu navigation
    void openMenu(const std::string& menuName);
    void closeMenu();
    void closeAllMenus();  // Close all menus without opening main menu
    void returnToMainMenu();
    
    // Check if a menu is active
    bool isMenuActive() const;
    
    // Check if game should be paused
    bool shouldPauseGame();
    
    // Get Engine instance
    Engine* getEngine() const { return engine; }
    
    // Menu factory (creates menu instances)
    std::unique_ptr<Menu> createMenu(const std::string& menuName);
    
private:
    Engine* engine;
    std::stack<std::unique_ptr<Menu>> menuStack;
    bool initialized;
    bool menuJustOpened;  // Track if a menu was opened this frame
    
    // Helper to get current menu
    Menu* getCurrentMenu() const;
};

#endif // MENU_MANAGER_H

