#pragma once

#include <SFML/Graphics.hpp>
#include <functional>
#include <string>
#include "Config.hpp"  

class Player {
public:
    sf::Vector2f pos{
        SPAWN_TX * TILE + TILE * 0.5f,
        SPAWN_TY * TILE + TILE * 0.5f
    };

    bool loaded = false;

    bool sprint = false;   // 双击方向键触发的冲刺
    int sprintDir = -1;    // 冲刺方向（0下 1上 2右 3左）

    bool load(const std::string& path);
    sf::FloatRect hitbox() const;

    void update(float dt,
        const std::function<bool(const sf::FloatRect&)>& collideFn);

    void draw(sf::RenderTarget& rt);
    sf::FloatRect debugBox() const;

private:
    sf::Texture tex;
    sf::Sprite sprite;

    int frameW = 16;
    int frameH = 32;
    int framesInRow = 4;
    int dirRow = 0;
    int dirFrames = 4;
    int frame = 0;
    int animStep = 0;
    float animTimer = 0.f;
    float scale = 2.f;
};