#ifndef MENU_H
#define MENU_H

#include <string>
#include <vector>
#include <functional>

// Forward declarations
class MenuManager;

// Menu item structure
struct MenuItem {
    std::string text;
    bool enabled;
    bool isSelectable;
    std::function<void()> onSelect;
    
    MenuItem(const std::string& t, bool sel, std::function<void()> callback)
        : text(t), enabled(true), isSelectable(sel), onSelect(callback) {}
};

// Base class for all menus
class Menu {
public:
    Menu(MenuManager* manager);
    virtual ~Menu() = default;
    
    // Update menu (handles input, etc.)
    virtual void update(float deltaTime);
    
    // Called when menu is opened
    virtual void onOpen();
    
    // Called when menu is closed
    virtual void onClose();
    
    // Handle navigation input
    virtual void handleNavigation(float upDown, float leftRight);
    virtual void handleConfirm();
    virtual void handleCancel();
    virtual void handleMouse(int mouseX, int mouseY, bool mousePressed);
    
    // Get menu items (for rendering)
    const std::vector<MenuItem>& getItems() const { return items; }
    int getSelectedIndex() const { return selectedIndex; }
    int getHoveredIndex() const { return hoveredIndex; }
    
    // Get menu title
    const std::string& getTitle() const { return title; }
    
protected:
    MenuManager* menuManager;
    std::string title;
    std::vector<MenuItem> items;
    int selectedIndex;
    int hoveredIndex;
    bool mouseMode;
    
    // Helper methods for derived classes
    void setTitle(const std::string& t) { title = t; }
    void addItem(const std::string& text, bool selectable, std::function<void()> callback);
    void addItem(const std::string& text, std::function<void()> callback);
    void clearItems();
    
    // Navigation helpers
    void selectNext();
    void selectPrevious();
    void selectItem(int index);
    void activateItem(int index);
};

#endif // MENU_H

