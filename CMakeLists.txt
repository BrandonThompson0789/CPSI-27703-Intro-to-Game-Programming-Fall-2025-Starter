cmake_minimum_required(VERSION 3.21)
project(CppSdl2Box2dTinyxml2Starter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(box2d CONFIG REQUIRED)
find_package(tinyxml2 CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Collect all source files recursively from src directory

# Create executable
add_executable(demo 
    src/main.cpp 
    src/Object.h
    src/Object.cpp
    src/Engine.h
    src/Engine.cpp
    src/Box2DDebugDraw.h
    src/Box2DDebugDraw.cpp
    src/SpriteManager.h
    src/SpriteManager.cpp
    src/InputManager.h
    src/InputManager.cpp
    src/InputConfig.h
    src/InputConfig.cpp
    src/ObjectSerializer.h
    src/CollisionManager.h
    src/CollisionManager.cpp
    src/PhysicsMaterial.h
    src/PhysicsMaterial.cpp
    src/components/Component.h
    src/components/ComponentLibrary.h
    src/components/ComponentLibrary.cpp
    src/components/BodyComponent.h
    src/components/BodyComponent.cpp
    src/components/HealthComponent.h
    src/components/HealthComponent.cpp
    src/components/SpriteComponent.h
    src/components/SpriteComponent.cpp
    src/components/ExplodeOnDeathComponent.h
    src/components/ExplodeOnDeathComponent.cpp
    src/components/InputComponent.h
    src/components/InputComponent.cpp
    src/components/PlayerMovementComponent.h
    src/components/PlayerMovementComponent.cpp
    src/components/CollisionDamageComponent.h
    src/components/CollisionDamageComponent.cpp
    src/components/BoxBehaviorComponent.h
    src/components/BoxBehaviorComponent.cpp
    src/components/JointComponent.h
    src/components/JointComponent.cpp
    src/components/ViewGrabComponent.h
    src/components/ViewGrabComponent.cpp
)

# Link libraries
target_link_libraries(demo PRIVATE
    SDL2::SDL2
    SDL2_ttf::SDL2_ttf
    SDL2_image::SDL2_image
    SDL2_mixer::SDL2_mixer
    box2d::box2d
    tinyxml2::tinyxml2
    yaml-cpp::yaml-cpp
    nlohmann_json::nlohmann_json
)

# Define SDL_MAIN_HANDLED for MinGW
target_compile_definitions(demo PRIVATE SDL_MAIN_HANDLED)

# Copy assets and DLLs
# Glob all asset files to track changes
file(GLOB_RECURSE ASSET_FILES "${CMAKE_SOURCE_DIR}/assets/*")

# Create a custom target that always copies assets
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:demo>/assets
    COMMENT "Copying assets to build directory..."
    DEPENDS ${ASSET_FILES}
)

# Make demo depend on copy_assets so assets are copied before running
add_dependencies(demo copy_assets)

# Copy DLLs after building demo
add_custom_command(TARGET demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:demo> $<TARGET_FILE_DIR:demo>
    COMMAND_EXPAND_LISTS
)
