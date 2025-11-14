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
    src/BackgroundManager.h
    src/BackgroundManager.cpp
    src/InputManager.h
    src/InputManager.cpp
    src/InputConfig.h
    src/InputConfig.cpp
    src/SoundManager.h
    src/SoundManager.cpp
    src/ObjectSerializer.h
    src/CollisionManager.h
    src/CollisionManager.cpp
    src/SensorEventManager.h
    src/SensorEventManager.cpp
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
    src/components/SoundComponent.h
    src/components/SoundComponent.cpp
    src/components/ExplodeOnDeathComponent.h
    src/components/ExplodeOnDeathComponent.cpp
    src/components/InputComponent.h
    src/components/InputComponent.cpp
    src/components/InteractComponent.h
    src/components/InteractComponent.cpp
    src/components/SensorComponent.h
    src/components/SensorComponent.cpp
    src/components/SensorTypes.h
    src/components/SensorTypes.cpp
    src/components/behaviors/StandardMovementBehaviorComponent.h
    src/components/behaviors/StandardMovementBehaviorComponent.cpp
    src/components/behaviors/GrabBehaviorComponent.h
    src/components/behaviors/GrabBehaviorComponent.cpp
    src/components/behaviors/ThrowBehaviorComponent.h
    src/components/behaviors/ThrowBehaviorComponent.cpp
    src/components/behaviors/TankMovementBehaviorComponent.h
    src/components/behaviors/TankMovementBehaviorComponent.cpp
    src/components/CollisionDamageComponent.h
    src/components/CollisionDamageComponent.cpp
    src/components/JointComponent.h
    src/components/JointComponent.cpp
    src/components/ViewGrabComponent.h
    src/components/ViewGrabComponent.cpp
    src/components/DeathTriggerComponent.h
    src/components/DeathTriggerComponent.cpp
    src/components/RailComponent.h
    src/components/RailComponent.cpp
    src/components/ObjectSpawnerComponent.h
    src/components/ObjectSpawnerComponent.cpp
)

# Link libraries
target_link_libraries(demo PRIVATE
    SDL2::SDL2
    $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static>
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
    $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer>,SDL2_mixer::SDL2_mixer,SDL2_mixer::SDL2_mixer-static>
    box2d::box2d
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
if(WIN32)
    add_custom_command(TARGET demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:demo> $<TARGET_FILE_DIR:demo>
        COMMAND_EXPAND_LISTS
    )
endif()

# Create server_manager executable
add_executable(server_manager
    src/server_manager/server_manager_main.cpp
    src/server_manager/ServerManager.h
    src/server_manager/ServerManager.cpp
    src/server_manager/NetworkUtils.h
    src/server_manager/NetworkUtils.cpp
)

# Link system libraries for networking
if(WIN32)
    target_link_libraries(server_manager PRIVATE ws2_32)
else()
    target_link_libraries(server_manager PRIVATE pthread)
endif()
