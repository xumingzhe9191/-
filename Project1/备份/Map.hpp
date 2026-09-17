#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

inline constexpr int MAP_W = 15;
inline constexpr int MAP_H = 10;

extern const std::vector<std::string> MAP;

bool isSolidTile(char c);

void drawTile(sf::RenderTarget& rt, char c, int tx, int ty);