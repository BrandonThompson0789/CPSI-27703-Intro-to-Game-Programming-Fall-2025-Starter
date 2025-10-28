# Sprite System Usage Guide

## Overview
The sprite system consists of two main components:
1. **SpriteManager** - Singleton that loads and caches sprite data and textures
2. **SpriteComponent** - Component that renders sprites on game objects

## Features
✅ Load sprite data from JSON file  
✅ Multiple frames per sprite for animation  
✅ Sprite data loaded only once and cached  
✅ Texture atlas support (multiple sprites per texture)  
✅ Optional texture unloading  
✅ Animation control (play, stop, speed)  
✅ Horizontal/vertical flipping  
✅ Alpha transparency  

---

## Setup

### 1. Initialize SpriteManager in Engine

In your `Engine::init()` method, add:

```cpp
#include "components/SpriteManager.h"

void Engine::init() {
    // ... existing SDL initialization ...
    
    // Initialize sprite manager with renderer
    SpriteManager::getInstance().init(renderer, "assets/spriteData.json");
}
```

### 2. Cleanup on Exit

In your `Engine::cleanup()` method:

```cpp
void Engine::cleanup() {
    SpriteManager::getInstance().cleanup();
    // ... existing cleanup code ...
}
```

---

## JSON Format

### Basic Structure

```json
{
  "textures": {
    "texture_filename.png": {
      "sprites": {
        "sprite_name": {
          "x": 0,
          "y": 0,
          "w": 32,
          "h": 32
        }
      }
    }
  }
}
```

### With Animation Frames

```json
{
  "textures": {
    "characters.png": {
      "sprites": {
        "player_walk": {
          "x": 0,
          "y": 0,
          "w": 32,
          "h": 32,
          "frames": [
            { "x": 0, "y": 0, "w": 32, "h": 32 },
            { "x": 32, "y": 0, "w": 32, "h": 32 },
            { "x": 64, "y": 0, "w": 32, "h": 32 },
            { "x": 96, "y": 0, "w": 32, "h": 32 }
          ]
        }
      }
    }
  }
}
```

**Notes:**
- `x, y` - Position in the texture atlas
- `w, h` - Width and height of the sprite
- `frames` - Optional array of frames for animation
- If `frames` is omitted, the sprite is treated as a single frame

---

## Using SpriteComponent

### Basic Usage

```cpp
#include "components/SpriteComponent.h"

class MyObject : public Object {
private:
    std::unique_ptr<SpriteComponent> spriteComp;

public:
    MyObject() {
        // Create sprite component with sprite name from JSON
        spriteComp = std::make_unique<SpriteComponent>(*this, "player");
    }

    void update() override {
        spriteComp->update();
    }

    void render(SDL_Renderer* renderer) override {
        spriteComp->draw();
    }
};
```

### With Animation

```cpp
// Play animation (looping)
spriteComp->playAnimation(true);

// Set animation speed (10 frames per second)
spriteComp->setAnimationSpeed(10.0f);

// Play animation once (no loop)
spriteComp->playAnimation(false);

// Stop animation
spriteComp->stopAnimation();

// Set specific frame
spriteComp->setFrame(0);
```

### Rendering Options

```cpp
// Flip horizontally (mirror)
spriteComp->setFlipHorizontal(true);

// Flip vertically
spriteComp->setFlipVertical(true);

// Set transparency (0 = fully transparent, 255 = fully opaque)
spriteComp->setAlpha(128);
```

---

## Advanced: Direct SpriteManager Usage

### Render Sprite Directly

```cpp
SpriteManager::getInstance().renderSprite(
    "player",           // sprite name
    0,                  // frame index
    100,                // x position
    200,                // y position
    45.0f,              // rotation angle
    SDL_FLIP_NONE,      // flip flags
    255                 // alpha
);
```

### Get Sprite Data

```cpp
const SpriteData* data = SpriteManager::getInstance().getSpriteData("player");
if (data) {
    int frameCount = data->getFrameCount();
    SpriteFrame frame = data->getFrame(0);
    // Use frame.x, frame.y, frame.w, frame.h
}
```

### Texture Management

```cpp
// Get texture (loads if not cached)
SDL_Texture* texture = SpriteManager::getInstance().getTexture("characters.png");

// Unload specific texture
SpriteManager::getInstance().unloadTexture("characters.png");

// Unload all textures and sprite data
SpriteManager::getInstance().unloadAll();
```

---

## Example Object with SpriteComponent

```cpp
#include "Object.h"
#include "components/SpriteComponent.h"
#include <memory>

class AnimatedPlayer : public Object {
private:
    std::unique_ptr<SpriteComponent> sprite;
    float speed = 5.0f;
    bool movingRight = true;

public:
    AnimatedPlayer() {
        sprite = std::make_unique<SpriteComponent>(*this, "player_walk");
        sprite->setAnimationSpeed(8.0f);
        sprite->playAnimation(true);
    }

    void update() override {
        // Update sprite animation
        sprite->update();

        // Move player
        if (movingRight) {
            position.x += speed;
            sprite->setFlipHorizontal(false);
            if (position.x > 700) movingRight = false;
        } else {
            position.x -= speed;
            sprite->setFlipHorizontal(true);
            if (position.x < 100) movingRight = true;
        }
    }

    void render(SDL_Renderer* renderer) override {
        sprite->draw();
    }
};
```

---

## Tips & Best Practices

1. **Texture Atlases** - Pack multiple sprites into one texture to reduce texture switches
2. **Single Load** - SpriteManager ensures each texture is loaded only once
3. **Memory Management** - Call `unloadAll()` when switching levels or scenes
4. **Frame Rate** - Animation speed is independent of frame rate
5. **Sprite Names** - Use descriptive names like "player_walk", "enemy_idle", etc.

---

## Troubleshooting

**Sprite not showing?**
- Check if sprite name in code matches name in JSON
- Verify texture file exists in `assets/` directory
- Ensure SpriteManager is initialized with correct renderer
- Check console for error messages

**Animation not playing?**
- Call `playAnimation(true)` to start
- Ensure sprite has multiple frames in JSON
- Call `sprite->update()` in your object's update method

**Texture loading error?**
- Make sure SDL_image is initialized
- Verify image file format is supported (PNG, JPG, BMP, etc.)
- Check file path is relative to executable location

