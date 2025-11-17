#include "Menu.h"
#include "MenuManager.h"
#include "../Engine.h"
#include <algorithm>

Menu::Menu(MenuManager* manager)
    : menuManager(manager), selectedIndex(0), hoveredIndex(-1), mouseMode(false) {
}

void Menu::update(float deltaTime) {
    // Default implementation does nothing
    // Derived classes can override to add animations, etc.
}

void Menu::onOpen() {
    selectedIndex = 0;
    hoveredIndex = -1;
    mouseMode = false;
    
    // Find first enabled selectable item
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].isSelectable && items[i].enabled) {
            selectedIndex = static_cast<int>(i);
            break;
        }
    }
}

void Menu::onClose() {
    // Default implementation does nothing
}

void Menu::handleNavigation(float upDown, float leftRight) {
    if (items.empty()) return;
    
    mouseMode = false; // Switch to keyboard/controller mode
    
    // Handle vertical navigation (up/down)
    // upDown > 0.5f means DOWN was pressed, so move DOWN (selectNext)
    // upDown < -0.5f means UP was pressed, so move UP (selectPrevious)
    if (upDown > 0.5f) {
        selectNext();  // Down pressed -> go down (select next)
    } else if (upDown < -0.5f) {
        selectPrevious();  // Up pressed -> go up (select previous)
    }
}

void Menu::handleConfirm() {
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(items.size())) {
        activateItem(selectedIndex);
    }
}

void Menu::handleCancel() {
    // Default cancel behavior - return to main menu
    // Derived classes can override
    if (menuManager) {
        menuManager->returnToMainMenu();
    }
}

void Menu::handleMouse(int mouseX, int mouseY, bool mousePressed) {
    // Default mouse handling implementation
    mouseMode = true;
    
    if (!menuManager || !menuManager->getEngine()) {
        return;
    }
    
    // Calculate menu bounds (same as rendering)
    // Use Engine's static screen dimensions
    int screenWidth = Engine::screenWidth;
    int screenHeight = Engine::screenHeight;
    
    int menuWidth = 400;
    int menuHeight = static_cast<int>(items.size() * 60) + 100;
    int menuX = (screenWidth - menuWidth) / 2;
    int menuY = (screenHeight - menuHeight) / 2;
    
    // Check if mouse is over menu
    if (mouseX >= menuX && mouseX < menuX + menuWidth &&
        mouseY >= menuY + 80 && mouseY < menuY + menuHeight) {
        
        int itemHeight = 50;
        int yOffset = menuY + 80;
        int itemIndex = (mouseY - yOffset) / itemHeight;
        
        if (itemIndex >= 0 && itemIndex < static_cast<int>(items.size())) {
            hoveredIndex = itemIndex;
            
            // If clicked and item is enabled, select it
            if (mousePressed && items[itemIndex].enabled && items[itemIndex].isSelectable) {
                selectedIndex = itemIndex;
                activateItem(itemIndex);
            }
        } else {
            hoveredIndex = -1;
        }
    } else {
        hoveredIndex = -1;
    }
}

void Menu::addItem(const std::string& text, bool selectable, std::function<void()> callback) {
    items.emplace_back(text, selectable, callback);
}

void Menu::addItem(const std::string& text, std::function<void()> callback) {
    items.emplace_back(text, true, callback);
}

void Menu::clearItems() {
    items.clear();
    selectedIndex = 0;
    hoveredIndex = -1;
}

void Menu::selectNext() {
    if (items.empty()) return;
    
    int startIndex = selectedIndex;
    int attempts = 0;
    
    do {
        selectedIndex = (selectedIndex + 1) % static_cast<int>(items.size());
        attempts++;
        
        if (items[selectedIndex].isSelectable && items[selectedIndex].enabled) {
            return;
        }
    } while (selectedIndex != startIndex && attempts < static_cast<int>(items.size()));
    
    // If no enabled item found, keep current selection
    selectedIndex = startIndex;
}

void Menu::selectPrevious() {
    if (items.empty()) return;
    
    int startIndex = selectedIndex;
    int attempts = 0;
    
    do {
        selectedIndex = (selectedIndex - 1 + static_cast<int>(items.size())) % static_cast<int>(items.size());
        attempts++;
        
        if (items[selectedIndex].isSelectable && items[selectedIndex].enabled) {
            return;
        }
    } while (selectedIndex != startIndex && attempts < static_cast<int>(items.size()));
    
    // If no enabled item found, keep current selection
    selectedIndex = startIndex;
}

void Menu::selectItem(int index) {
    if (index >= 0 && index < static_cast<int>(items.size()) && 
        items[index].isSelectable && items[index].enabled) {
        selectedIndex = index;
    }
}

void Menu::activateItem(int index) {
    if (index >= 0 && index < static_cast<int>(items.size()) && 
        items[index].isSelectable && items[index].enabled && items[index].onSelect) {
        items[index].onSelect();
    }
}

