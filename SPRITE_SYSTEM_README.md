# Sprite System Implementation

## ✅ What Was Created

### Core System Files
1. **`src/components/SpriteManager.h`** - Singleton sprite data and texture manager
2. **`src/components/SpriteManager.cpp`** - Implementation with JSON loading and caching
3. **`src/components/SpriteComponent.h`** - Component for rendering sprites with animation
4. **`src/components/SpriteComponent.cpp`** - Implementation with animation support
5. **`src/components/BodyComponent.h`** - Updated with public inheritance fix
6. **`src/components/BodyComponent.cpp`** - Physics component implementation

### Configuration Files
7. **`assets/spriteData.json`** - Example sprite data configuration
8. **`CMakeLists.txt`** - Updated to include all component files

### Documentation
9. **`src/components/SPRITE_USAGE.md`** - Complete usage guide with examples
10. **`src/components/INTEGRATION_EXAMPLE.cpp`** - Code examples for integration

---

## 🎯 Key Features Implemented

✅ **Single-Load System** - Sprite data and textures loaded once and cached  
✅ **JSON Configuration** - Define sprites and animations in `spriteData.json`  
✅ **Texture Atlases** - Multiple sprites per texture supported  
✅ **Multi-Frame Animation** - Define multiple frames per sprite for animation  
✅ **Animation Control** - Play, stop, set speed, loop/no-loop  
✅ **Rendering Options** - Flip horizontal/vertical, alpha transparency, rotation  
✅ **Optional Unloading** - Unload specific textures or all at once  
✅ **Component-Based** - Integrates with existing Component system  

---

## 📁 JSON Structure

```json
{
  "textures": {
    "characters.png": {
      "sprites": {
        "player": {
          "x": 0, "y": 0, "w": 32, "h": 32,
          "frames": [
            { "x": 0, "y": 0, "w": 32, "h": 32 },
            { "x": 32, "y": 0, "w": 32, "h": 32 },
            { "x": 64, "y": 0, "w": 32, "h": 32 }
          ]
        },
        "enemy": {
          "x": 0, "y": 32, "w": 32, "h": 32
        }
      }
    }
  }
}
```

**Field Descriptions:**
- **textures** - Object containing texture files
  - **texture_name.png** - The texture file name (in `assets/` folder)
    - **sprites** - Object containing sprite definitions
      - **sprite_name** - Unique name for the sprite
        - **x, y** - Position in texture atlas (pixels)
        - **w, h** - Width and height (pixels)
        - **frames** - (Optional) Array of frames for animation

---

## 🚀 Quick Start

### Step 1: Initialize SpriteManager

In `Engine::init()`:

```cpp
#include "components/SpriteManager.h"

void Engine::init() {
    // ... existing SDL initialization ...
    
    // Initialize sprite system
    SpriteManager::getInstance().init(renderer, "assets/spriteData.json");
}
```

### Step 2: Cleanup on Exit

In `Engine::cleanup()`:

```cpp
void Engine::cleanup() {
    SpriteManager::getInstance().cleanup();
    // ... rest of cleanup ...
}
```

### Step 3: Use in Your Objects

```cpp
#include "components/SpriteComponent.h"

class MyPlayer : public Object {
private:
    std::unique_ptr<SpriteComponent> sprite;

public:
    MyPlayer() {
        sprite = std::make_unique<SpriteComponent>(*this, "player");
        sprite->setAnimationSpeed(10.0f);
        sprite->playAnimation(true);
    }

    void update() override {
        sprite->update();
    }

    void render(SDL_Renderer* renderer) override {
        sprite->draw();
    }
};
```

---

## 🔧 Building the Project

The CMakeLists.txt has been updated to include all component files. Simply rebuild:

```bash
cd build/win-mingw-debug
cmake --build .
```

---

## 📖 Full Documentation

See **`src/components/SPRITE_USAGE.md`** for:
- Detailed API reference
- Advanced usage examples
- Animation control
- Texture management
- Troubleshooting

See **`src/components/INTEGRATION_EXAMPLE.cpp`** for:
- Integration patterns
- Multi-component objects
- State-based sprite switching
- Level loading examples

---

## 🎨 Texture Atlas Tips

### Recommended Layout

```
[frame1][frame2][frame3][frame4]  <- Player walk animation
[frame1][frame2]                  <- Enemy idle animation
[coin][heart][star]               <- Static items
```

### Benefits
- Reduced texture switches (better performance)
- Easier to organize related sprites
- Single file to manage per sprite sheet

### Tools for Creating Atlases
- TexturePacker
- Shoebox
- Free Texture Packer
- GIMP with Grid

---

## 💡 Usage Patterns

### Pattern 1: Static Sprite
```cpp
sprite = std::make_unique<SpriteComponent>(*this, "coin");
```

### Pattern 2: Looping Animation
```cpp
sprite = std::make_unique<SpriteComponent>(*this, "player_walk");
sprite->setAnimationSpeed(10.0f);
sprite->playAnimation(true);  // Loop
```

### Pattern 3: One-Shot Animation
```cpp
sprite = std::make_unique<SpriteComponent>(*this, "explosion");
sprite->setAnimationSpeed(20.0f);
sprite->playAnimation(false);  // No loop
```

### Pattern 4: Manual Frame Control
```cpp
sprite->stopAnimation();
sprite->setFrame(0);  // Show specific frame
```

---

## 🧪 Testing the System

1. **Create test texture** - Make a simple PNG with a 32x32 sprite
2. **Update spriteData.json** - Add your sprite definition
3. **Create test object** - Use SpriteComponent with your sprite name
4. **Run and verify** - Check console for loading messages

---

## 🔍 Console Output

When working correctly, you'll see:
```
SpriteManager: Loaded 2 sprites from assets/spriteData.json
SpriteManager: Loaded texture: assets/characters.png
```

If there are issues:
```
SpriteManager: Could not open sprite data file: assets/spriteData.json
SpriteManager: Sprite not found: invalid_name
SpriteManager: Failed to load image: assets/missing.png
```

---

## 🎯 Next Steps

1. ✅ Add sprite initialization to Engine::init()
2. ✅ Add cleanup to Engine::cleanup()
3. ✅ Create your texture atlas image(s)
4. ✅ Define sprites in spriteData.json
5. ✅ Integrate SpriteComponent into your game objects
6. ✅ Test and iterate!

---

## 📝 Notes

- **Thread Safety**: SpriteManager is NOT thread-safe (designed for single-threaded game loop)
- **Memory**: Textures stay in memory until explicitly unloaded
- **Performance**: Sprite lookup is O(1) using hash maps
- **Coordinate System**: Origin (0,0) is top-left corner of texture

---

## 🐛 Troubleshooting

**Q: My sprite isn't showing up**
- Check console for error messages
- Verify sprite name matches JSON exactly (case-sensitive)
- Ensure texture file exists in `assets/` folder
- Confirm SpriteManager.init() was called

**Q: Animation isn't playing**
- Make sure you defined `frames` array in JSON
- Call `sprite->update()` in your update method
- Verify `playAnimation(true)` was called

**Q: Texture loading failed**
- Check SDL_image is properly linked
- Verify image format is supported
- Make sure path is relative to executable location

**Q: Performance issues**
- Use texture atlases to reduce texture switches
- Unload unused textures when switching levels
- Consider using texture streaming for large games

---

## 📞 Support

For issues or questions:
1. Check `SPRITE_USAGE.md` for API details
2. Review `INTEGRATION_EXAMPLE.cpp` for patterns
3. Check console output for error messages
4. Verify JSON syntax at jsonlint.com

---

## 🏆 Advanced Topics

### Custom Render Parameters
```cpp
SpriteManager::getInstance().renderSprite(
    "player", 0, 100, 200, 
    45.0f,              // rotation angle
    SDL_FLIP_HORIZONTAL, // flip
    128                  // alpha (50% transparent)
);
```

### Getting Sprite Info
```cpp
const SpriteData* data = SpriteManager::getInstance().getSpriteData("player");
int frameCount = data->getFrameCount();
SpriteFrame frame = data->getFrame(0);
```

### Dynamic Texture Loading
```cpp
// Load texture on demand
SDL_Texture* tex = SpriteManager::getInstance().getTexture("new_texture.png");

// Unload when done
SpriteManager::getInstance().unloadTexture("new_texture.png");
```

### Memory Management
```cpp
// Clear everything when switching levels
SpriteManager::getInstance().unloadAll();

// Reload for new level
SpriteManager::getInstance().loadSpriteData("assets/level2_sprites.json");
```

---

## ✨ Enjoy Your New Sprite System!

