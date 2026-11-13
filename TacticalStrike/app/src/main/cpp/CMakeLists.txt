cmake_minimum_required(VERSION 3.22.1)
project(tacticalstrike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(tacticalstrike SHARED
    native-lib.cpp
    GameEngine.cpp
    Renderer.cpp
    ShaderManager.cpp
    TextureManager.cpp
    ModelLoader.cpp
    Camera.cpp
    Player.cpp
    WeaponSystem.cpp
    EnemySystem.cpp
    EnemyAI.cpp
    Physics.cpp
    Collision.cpp
    Animation.cpp
    AudioSystem.cpp
    ParticleSystem.cpp
    LightingSystem.cpp
    LevelManager.cpp
    MissionManager.cpp
    SaveManager.cpp
    InputManager.cpp
)

target_include_directories(tacticalstrike PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_library(android-lib android)
find_library(egl-lib EGL)
find_library(glesv3-lib GLESv3)
find_library(opensles-lib OpenSLES)
find_library(log-lib log)

target_link_libraries(tacticalstrike
    ${android-lib}
    ${egl-lib}
    ${glesv3-lib}
    ${opensles-lib}
    ${log-lib}
    m
)
