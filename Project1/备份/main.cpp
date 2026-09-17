#define _CRT_SECURE_NO_WARNINGS
#include "Config.hpp"
#include "Map.hpp"
#include "Player.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

int main() {
    sf::RenderWindow window(
        sf::VideoMode(WIN_W, WIN_H),
        "Pixel RPG - Sophia",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    // 预渲染地图
    sf::RenderTexture mapRT;
    if (!mapRT.create(MAP_W * TILE, MAP_H * TILE)) {
        std::cerr << "创建地图 RenderTexture 失败" << std::endl;
        return -1;
    }

    mapRT.clear(sf::Color(30, 40, 30));
    for (int y = 0; y < MAP_H; ++y)
        for (int x = 0; x < MAP_W; ++x)
            drawTile(mapRT, MAP[y][x], x, y);
    mapRT.display();

    sf::Sprite mapSprite(mapRT.getTexture());

    // 玩家
    Player player;
    if (!player.load(SPRITE_PATH)) {
        std::cerr << "无法加载角色图片：\n" << SPRITE_PATH << std::endl;
    }

    // 碰撞
    auto collide = [&](const sf::FloatRect& r) -> bool {
        const int x0 = (int)std::floor(r.left / TILE);
        const int y0 = (int)std::floor(r.top / TILE);
        const int x1 = (int)std::floor((r.left + r.width - 1.f) / TILE);
        const int y1 = (int)std::floor((r.top + r.height - 1.f) / TILE);

        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H)
                    return true;
                if (isSolidTile(MAP[y][x]))
                    return true;
            }
        return false;
        };

    sf::View view(sf::FloatRect(0.f, 0.f, (float)WIN_W, (float)WIN_H));

    bool showDebug = false;
    sf::Clock clock;

    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed)
                window.close();

            if (e.type == sf::Event::KeyPressed) {
                if (e.key.code == sf::Keyboard::Escape)
                    window.close();
                if (e.key.code == sf::Keyboard::F1)
                    showDebug = !showDebug;
            }
        }

        float dt = clock.restart().asSeconds();
        if (dt > 0.1f) dt = 0.1f;

        player.update(dt, collide);

        // 相机
        const float mapPX = (float)(MAP_W * TILE);
        const float mapPY = (float)(MAP_H * TILE);
        const float halfW = WIN_W * 0.5f;
        const float halfH = WIN_H * 0.5f;

        float camX, camY;
        if (mapPX <= WIN_W) camX = mapPX * 0.5f;
        else camX = std::max(halfW, std::min(player.pos.x, mapPX - halfW));

        if (mapPY <= WIN_H) camY = mapPY * 0.5f;
        else camY = std::max(halfH, std::min(player.pos.y, mapPY - halfH));

        view.setCenter(std::round(camX), std::round(camY));
        window.setView(view);

        window.clear(sf::Color(18, 24, 18));
        window.draw(mapSprite);
        player.draw(window);

        if (showDebug) {
            sf::FloatRect b = player.debugBox();
            sf::RectangleShape r(sf::Vector2f(b.width, b.height));
            r.setPosition(b.left, b.top);
            r.setFillColor(sf::Color(255, 0, 0, 60));
            r.setOutlineColor(sf::Color::Red);
            r.setOutlineThickness(1.f);
            window.draw(r);
        }

        window.display();
    }

    return 0;
}