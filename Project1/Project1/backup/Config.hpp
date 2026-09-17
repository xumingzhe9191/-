#pragma once

#include <string>

inline constexpr int TILE = 64;
// 地图绘制用小格尺寸（16px），随后最近邻放大 4 倍获得像素颗粒感
inline constexpr int TILE_RENDER = TILE / 4;
inline constexpr unsigned WIN_W = 960;
inline constexpr unsigned WIN_H = 640;

inline constexpr float PLAYER_SPEED = 225.f;  // 1.5x of original 150
inline constexpr float DOUBLE_TAP_WINDOW = 0.25f;  // 双击方向键判定窗口（秒）
inline constexpr float SPRINT_MULT = 2.f;          // 冲刺倍速
inline constexpr float ANIM_INTERVAL = 0.13f;

inline constexpr int SPAWN_TX = 6;
inline constexpr int SPAWN_TY = 5;

inline const std::string SPRITE_PATH = "assets/sophia.png";