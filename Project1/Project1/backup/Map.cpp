#define _CRT_SECURE_NO_WARNINGS
#include "Map.hpp"
#include "Config.hpp"
#include <SFML/Graphics.hpp>
#include <cmath>
#include <cstring>

// ========== pixel helpers ==========
static void pxSet(sf::Image& img, int ox, int oy, int x, int y, sf::Color c) {
    int px = ox + x, py = oy + y;
    if (px >= 0 && py >= 0 && px < (int)img.getSize().x && py < (int)img.getSize().y)
        img.setPixel(px, py, c);
}
static void pxRect(sf::Image& img, int ox, int oy, int x0, int y0, int x1, int y1, sf::Color c) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            pxSet(img, ox, oy, x, y, c);
}
static unsigned hash2(int x, int y) {
    unsigned h = (unsigned)(x * 374761393 + y * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// ========== digit font (3x5) ==========
static const int DIGIT_FONT[10][5] = {
    {0b111,0b101,0b101,0b101,0b111},
    {0b010,0b110,0b010,0b010,0b111},
    {0b111,0b001,0b111,0b100,0b111},
    {0b111,0b001,0b111,0b001,0b111},
    {0b101,0b101,0b111,0b001,0b001},
    {0b111,0b100,0b111,0b001,0b111},
    {0b111,0b100,0b111,0b101,0b111},
    {0b111,0b001,0b010,0b010,0b010},
    {0b111,0b101,0b111,0b101,0b111},
    {0b111,0b101,0b111,0b001,0b111},
};
static void drawDigitBig(sf::Image& img, int ox, int oy, int digit) {
    int s = 3;
    int w = 3 * s, h = 5 * s;
    int dx = (TILE_RENDER - w) / 2;
    int dy = (TILE_RENDER - h) / 2;
    for (int r = 0; r < 5; ++r) {
        int bits = DIGIT_FONT[digit][r];
        for (int c = 0; c < 3; ++c) {
            if (bits & (1 << (2 - c)))
                pxRect(img, ox, oy, dx + c*s, dy + r*s, dx + c*s + s - 1, dy + r*s + s - 1,
                       sf::Color(255, 240, 180));
        }
    }
}

// ========== scenes ==========
static const char* DORM_MAP[] = {
    "#BB.BB.BB...WW#",
    "#BB.BB.BB...WW#",
    "#bb.bb.bb...###",
    "#bb.bb.bb...###",
    "O.......Y...GG#",
    "O...........GG#",
    "#BB.BB.BB...###",
    "#BB.BB.BB...WW#",
    "#bb.bb.bb...WW#",
    "#bb.bb.bb...###",
};
static const int DORM_W = 15, DORM_H = 10;

static const char* HALLWAY_MAP[] = {
    "######################",
    "######################",
    "######################",
    "######################",
    "######################",
    "#....................M",
    "#....................M",
    "#.H....D....D....D...#",
    "#.H....D....D....D...#",
    "#.291..203..205..207.#",
    "######################",
};
static const int HALLWAY_W = 22, HALLWAY_H = 11;

static const char* BALCONY_MAP[] = {
    "rrrrrrrrrr",
    "r........r",
    "r...E....r",
    "r........r",
    "r........r",
    "r........r",
    "rrrrrrrrrr",
};
static const int BALCONY_W = 10, BALCONY_H = 7;

// 宿舍楼外（学校）碰撞地图：世界 32x28 格 = 2048x1792，与 assets/dorm_building_topdown.png 1:1 铺底。
// 行0-5：海/沙滩（实心）；行6-19：红砖楼体（格3-28 实心），两侧格0-2、29-31 为楼边人行道（可走）；
// 行20：楼体南墙，格14-18 开门廊通道，格16 为入口门 M（传送回走廊）；行21：门廊台阶下层（可走）；
// 行22-27：楼前广场（全宽可走）。
static const char* SCHOOL_MAP[] = {
    "################################",
    "################################",
    "################################",
    "################################",
    "################################",
    "################################",
    "...##########################...",
    "...##########################...",
    "...##########################...",
    "...##########################...",
    "...##########################...",
    "...##########################...",
    "...##########################...",
    "...##########################...",
    "...##########################...",
    "...##########################...",
    "...##########################...",
    "...##########################...",
    "...##########################...",
    "...##########################...",
    "...##############.....#######...",
    "...##############MMMMM#######...",
    "................................",
    "..............................CC",
    "..............................CC",
    "..............................CC",
    "..............................CC",
    "..............................CC",
};
static const int SCHOOL_W = 32, SCHOOL_H = 28;

// 校园大地图：世界 50x28 格，整图 assets/campus_map.png 铺底
// '#'=建筑实心，'.'=可走（道路/草地/操场），'C'=传送回宿舍楼外
static const char* CAMPUS_MAP[] = {
    "......................########....................",
    ".########.............########....................",
    ".########.............########....................",
    ".########.............########....................",
    ".########.............########....................",
    ".########.............########....................",
    "....1....................4........................",
    "..................................................",
    "..................................................",
    "..................................................",
    "###...............................................",
    "###P..............................................",
    "###...............................................",
    "C######...........#############...................",
    "C######...........#############...................",
    "C###3##...........#############...................",
    "C##2####....###################...................",
    "C#######....###################...................",
    ".#######....###################...................",
    ".#######....###################...................",
    ".#######......6...#############...................",
    ".#######..........#############...................",
    ".#######..........#############...................",
    "........................5.........................",
    "..................................................",
    "..................................................",
    "..................................................",
    "..................................................",
    "..................................................",
};
static const int CAMPUS_W = 50, CAMPUS_H = 28;

// 各建筑室内房间：四周墙，中间地板，南侧出口门 X
static const char* ROOM_MAP[] = {
    "####################",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#..................#",
    "#########X##########",
    "####################",
};
static const int ROOM_W = 20, ROOM_H = 12;

static int currentScene = SCENE_DORM;

int getCurrentScene() { return currentScene; }
const char* getSceneName(int id) {
    switch (id) {
        case SCENE_DORM: return "Dorm Room 291";
        case SCENE_HALLWAY: return "Dorm Hallway";
        case SCENE_BALCONY: return "Balcony";
        case SCENE_SCHOOL: return "School Campus";
        case SCENE_CAMPUS: return "Campus";
        case SCENE_ROOM_A: return "Classroom A";
        case SCENE_ROOM_B: return "Classroom B";
        case SCENE_ROOM_C: return "Classroom C";
        case SCENE_ROOM_D: return "Main Hall";
        case SCENE_ROOM_E: return "Library";
        case SCENE_ROOM_F: return "Clinic";
        default: return "Unknown";
    }
}
void setScene(int id) { currentScene = id; }

const std::vector<std::string>& getMap() {
    static std::vector<std::string> dorm(DORM_MAP, DORM_MAP + DORM_H);
    static std::vector<std::string> hall(HALLWAY_MAP, HALLWAY_MAP + HALLWAY_H);
    static std::vector<std::string> bal(BALCONY_MAP, BALCONY_MAP + BALCONY_H);
    static std::vector<std::string> school(SCHOOL_MAP, SCHOOL_MAP + SCHOOL_H);
    static std::vector<std::string> campus(CAMPUS_MAP, CAMPUS_MAP + CAMPUS_H);
    static std::vector<std::string> room(ROOM_MAP, ROOM_MAP + ROOM_H);
    switch (currentScene) {
        case SCENE_HALLWAY: return hall;
        case SCENE_BALCONY: return bal;
        case SCENE_SCHOOL: return school;
        case SCENE_CAMPUS: return campus;
        case SCENE_ROOM_A: case SCENE_ROOM_B: case SCENE_ROOM_C:
        case SCENE_ROOM_D: case SCENE_ROOM_E: case SCENE_ROOM_F: return room;
        default: return dorm;
    }
}
int getMapW() {
    switch (currentScene) {
        case SCENE_HALLWAY: return HALLWAY_W;
        case SCENE_BALCONY: return BALCONY_W;
        case SCENE_SCHOOL: return SCHOOL_W;
        case SCENE_CAMPUS: return CAMPUS_W;
        case SCENE_ROOM_A: case SCENE_ROOM_B: case SCENE_ROOM_C:
        case SCENE_ROOM_D: case SCENE_ROOM_E: case SCENE_ROOM_F: return ROOM_W;
        default: return DORM_W;
    }
}
int getMapH() {
    switch (currentScene) {
        case SCENE_HALLWAY: return HALLWAY_H;
        case SCENE_BALCONY: return BALCONY_H;
        case SCENE_SCHOOL: return SCHOOL_H;
        case SCENE_CAMPUS: return CAMPUS_H;
        case SCENE_ROOM_A: case SCENE_ROOM_B: case SCENE_ROOM_C:
        case SCENE_ROOM_D: case SCENE_ROOM_E: case SCENE_ROOM_F: return ROOM_H;
        default: return DORM_H;
    }
}

// ========== warp points ==========
struct Warp { int fromScene; char tile; int toScene; int spawnTx, spawnTy; };
static const Warp WARPS[] = {
    {SCENE_DORM, 'O', SCENE_HALLWAY, 2, 6},
    {SCENE_DORM, 'G', SCENE_BALCONY, 4, 2},
    {SCENE_HALLWAY, 'H', SCENE_DORM, 1, 5},
    {SCENE_BALCONY, 'E', SCENE_DORM, 11, 4},
    {SCENE_HALLWAY, 'M', SCENE_SCHOOL, 20, 24},
    {SCENE_SCHOOL, 'M', SCENE_HALLWAY, 20, 5},   // 楼外玻璃门直接进宿舍走廊
    {SCENE_SCHOOL, 'C', SCENE_CAMPUS, 10, 10},   // 楼外马路右边 → 校园大地图（落在横向道路上）
    {SCENE_CAMPUS, 'C', SCENE_SCHOOL, 30, 24},   // 校园左边缘 → 回楼外
    {SCENE_CAMPUS, 'P', SCENE_SCHOOL, 30, 24},   // 黄框门 → 宿舍外俯视图
    // 建筑入口 → 各自独立房间；房间出口 X → 回校园对应建筑
    {SCENE_CAMPUS, '1', SCENE_ROOM_A, 9, 9},
    {SCENE_ROOM_A, 'X', SCENE_CAMPUS, 4, 7},
    {SCENE_CAMPUS, '2', SCENE_ROOM_B, 9, 9},
    {SCENE_ROOM_B, 'X', SCENE_CAMPUS, 3, 15},
    {SCENE_CAMPUS, '3', SCENE_ROOM_C, 9, 9},
    {SCENE_ROOM_C, 'X', SCENE_CAMPUS, 4, 14},
    {SCENE_CAMPUS, '4', SCENE_ROOM_D, 9, 9},
    {SCENE_ROOM_D, 'X', SCENE_CAMPUS, 25, 7},
    {SCENE_CAMPUS, '5', SCENE_ROOM_E, 9, 9},
    {SCENE_ROOM_E, 'X', SCENE_CAMPUS, 24, 22},
    {SCENE_CAMPUS, '6', SCENE_ROOM_F, 9, 9},
    {SCENE_ROOM_F, 'X', SCENE_CAMPUS, 14, 20},
};
static const int WARP_COUNT = sizeof(WARPS) / sizeof(WARPS[0]);

int checkWarp(int tx, int ty) {
    const auto& m = getMap();
    if (ty < 0 || ty >= (int)m.size() || tx < 0 || tx >= (int)m[0].size()) return -1;
    char c = m[ty][tx];
    for (int i = 0; i < WARP_COUNT; ++i)
        if (WARPS[i].fromScene == currentScene && WARPS[i].tile == c)
            return WARPS[i].toScene;
    return -1;
}
void getWarpSpawn(int fromScene, int toScene, int& outTx, int& outTy) {
    for (int i = 0; i < WARP_COUNT; ++i)
        if (WARPS[i].fromScene == fromScene && WARPS[i].toScene == toScene) {
            outTx = WARPS[i].spawnTx; outTy = WARPS[i].spawnTy; return;
        }
    outTx = 1; outTy = 1;
}

// ========== broom interaction ==========
static bool broomPickedUp = false;
bool isBroomPickedUp() { return broomPickedUp; }
bool hasBroomAt(int tx, int ty) {
    if (broomPickedUp || currentScene != SCENE_DORM) return false;
    return (tx == 8 && ty == 4);
}
void pickUpBroom() { broomPickedUp = true; }

// ========== collision ==========
bool isSolidTile(char c) {
    return c == '#' || c == 'B' || c == 'b' || c == 'D' || c == 'r'
        || c == 'R' || c == 'w' || c == 'a' || c == '~' || c == 'T' || c == 'F';
}

// ========== tile drawers ==========
static void drawWallPx(sf::Image& img, int ox, int oy) {
    pxRect(img, ox, oy, 0, 0, 15, 15, sf::Color(180, 140, 90));
    for (int y = 0; y < 16; y += 4) {
        int off = (y / 4) % 2 == 0 ? 0 : 8;
        for (int x = -8; x < 16; x += 8) {
            int bx = x + off;
            pxRect(img, ox, oy, bx, y, bx + 6, y, sf::Color(150, 110, 65));
            pxRect(img, ox, oy, bx, y, bx, y + 3, sf::Color(150, 110, 65));
        }
    }
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (hash2(ox + x, oy + y) % 7 == 0)
                pxSet(img, ox, oy, x, y, sf::Color(165, 125, 75));
    pxRect(img, ox, oy, 0, 15, 15, 15, sf::Color(120, 85, 45));
}

static void drawFloorPx(sf::Image& img, int ox, int oy) {
    pxRect(img, ox, oy, 0, 0, 15, 15, sf::Color(140, 100, 60));
    for (int y = 0; y < 16; y += 4) {
        pxRect(img, ox, oy, 0, y, 15, y, sf::Color(115, 80, 45));
        int off = (y / 4) % 2 == 0 ? 0 : 6;
        for (int x = -6; x < 16; x += 10)
            pxRect(img, ox, oy, x + off, y, x + off, y + 3, sf::Color(115, 80, 45));
    }
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (hash2(ox + x, oy + y) % 9 == 0)
                pxSet(img, ox, oy, x, y, sf::Color(155, 115, 70));
}

static void drawDoorPx(sf::Image& img, int ox, int oy) {
    drawFloorPx(img, ox, oy);
    // bigger, thicker door frame
    pxRect(img, ox, oy, 1, 0, 14, 15, sf::Color(70, 45, 20));
    pxRect(img, ox, oy, 2, 1, 13, 14, sf::Color(135, 92, 48));
    pxRect(img, ox, oy, 2, 1, 13, 1, sf::Color(165, 120, 68));
    pxRect(img, ox, oy, 2, 7, 13, 7, sf::Color(90, 60, 28));
    pxRect(img, ox, oy, 2, 8, 13, 8, sf::Color(115, 78, 40));
    // big handle
    pxRect(img, ox, oy, 10, 9, 12, 11, sf::Color(230, 210, 130));
    pxRect(img, ox, oy, 10, 9, 10, 11, sf::Color(255, 240, 170));
}

static void drawGlassDoorPx(sf::Image& img, int ox, int oy) {
    drawFloorPx(img, ox, oy);
    pxRect(img, ox, oy, 0, 0, 15, 15, sf::Color(60, 70, 90));
    pxRect(img, ox, oy, 1, 1, 14, 14, sf::Color(125, 170, 205));
    pxRect(img, ox, oy, 1, 1, 14, 1, sf::Color(165, 205, 235));
    pxRect(img, ox, oy, 1, 1, 1, 14, sf::Color(165, 205, 235));
    pxRect(img, ox, oy, 7, 1, 8, 14, sf::Color(60, 70, 90));
    pxRect(img, ox, oy, 3, 3, 5, 6, sf::Color(195, 225, 250));
    pxRect(img, ox, oy, 10, 8, 12, 11, sf::Color(195, 225, 250));
    pxRect(img, ox, oy, 11, 9, 12, 10, sf::Color(230, 210, 130));
}

static void drawWindowPx(sf::Image& img, int ox, int oy) {
    drawWallPx(img, ox, oy);
    pxRect(img, ox, oy, 1, 2, 14, 13, sf::Color(55, 70, 90));
    pxRect(img, ox, oy, 2, 3, 13, 12, sf::Color(115, 165, 205));
    pxRect(img, ox, oy, 2, 3, 13, 3, sf::Color(155, 200, 235));
    pxRect(img, ox, oy, 2, 3, 2, 12, sf::Color(155, 200, 235));
    pxRect(img, ox, oy, 7, 3, 8, 12, sf::Color(55, 70, 90));
    pxRect(img, ox, oy, 2, 7, 13, 8, sf::Color(55, 70, 90));
    pxRect(img, ox, oy, 3, 4, 5, 5, sf::Color(190, 220, 245));
}

static void drawRailingPx(sf::Image& img, int ox, int oy) {
    pxRect(img, ox, oy, 0, 0, 15, 3, sf::Color(160, 160, 170));
    pxRect(img, ox, oy, 0, 0, 15, 0, sf::Color(200, 200, 210));
    pxRect(img, ox, oy, 0, 3, 15, 3, sf::Color(110, 110, 120));
    for (int x = 2; x < 16; x += 4) {
        pxRect(img, ox, oy, x, 4, x + 1, 15, sf::Color(140, 140, 150));
        pxRect(img, ox, oy, x, 4, x, 15, sf::Color(170, 170, 180));
    }
    pxRect(img, ox, oy, 0, 8, 15, 9, sf::Color(110, 110, 120));
}

// bigger hallway door - thick frame, full panel
static void drawHallDoorPx(sf::Image& img, int ox, int oy, bool isTop) {
    drawWallPx(img, ox, oy);
    if (isTop) {
        // top half of a tall door
        pxRect(img, ox, oy, 0, 3, 15, 15, sf::Color(65, 42, 18));
        pxRect(img, ox, oy, 1, 4, 14, 15, sf::Color(115, 80, 38));
        pxRect(img, ox, oy, 1, 4, 14, 4, sf::Color(145, 102, 55));
        pxRect(img, ox, oy, 1, 9, 14, 9, sf::Color(75, 50, 22));
        pxRect(img, ox, oy, 1, 10, 14, 10, sf::Color(100, 68, 32));
        // big handle plate
        pxRect(img, ox, oy, 9, 11, 12, 13, sf::Color(225, 205, 125));
        pxRect(img, ox, oy, 9, 11, 9, 13, sf::Color(250, 235, 165));
    } else {
        // bottom half
        pxRect(img, ox, oy, 0, 0, 15, 12, sf::Color(65, 42, 18));
        pxRect(img, ox, oy, 1, 0, 14, 11, sf::Color(115, 80, 38));
        pxRect(img, ox, oy, 1, 0, 14, 0, sf::Color(145, 102, 55));
        pxRect(img, ox, oy, 1, 5, 14, 5, sf::Color(75, 50, 22));
        pxRect(img, ox, oy, 1, 6, 14, 6, sf::Color(100, 68, 32));
    }
}

// balcony door (E) - door leading back to dorm
static void drawBalconyDoorPx(sf::Image& img, int ox, int oy) {
    drawFloorPx(img, ox, oy);
    pxRect(img, ox, oy, 1, 0, 14, 15, sf::Color(70, 85, 105));
    pxRect(img, ox, oy, 2, 1, 13, 14, sf::Color(120, 155, 185));
    pxRect(img, ox, oy, 2, 1, 13, 1, sf::Color(155, 190, 220));
    pxRect(img, ox, oy, 2, 1, 2, 14, sf::Color(155, 190, 220));
    pxRect(img, ox, oy, 2, 7, 13, 7, sf::Color(70, 85, 105));
    pxRect(img, ox, oy, 10, 9, 12, 11, sf::Color(225, 210, 130));
    pxRect(img, ox, oy, 4, 3, 6, 5, sf::Color(185, 215, 240));
}

// broom (1 tile, full and饱满)
static void drawBroomPx(sf::Image& img, int ox, int oy) {
    // drop shadow
    pxRect(img, ox, oy, 3, 13, 14, 15, sf::Color(0, 0, 0, 45));
    // wooden handle (diagonal)
    for (int i = 0; i < 9; ++i) {
        int hx = 2 + i;
        int hy = 1 + (int)(i * 0.85f);
        pxRect(img, ox, oy, hx, hy, hx + 1, hy + 1, sf::Color(135, 88, 42));
        pxRect(img, ox, oy, hx, hy, hx, hy + 1, sf::Color(165, 110, 58));
    }
    // handle cap
    pxRect(img, ox, oy, 1, 0, 4, 3, sf::Color(115, 72, 32));
    // metal band
    pxRect(img, ox, oy, 9, 8, 14, 10, sf::Color(155, 155, 165));
    pxRect(img, ox, oy, 9, 8, 14, 8, sf::Color(195, 195, 205));
    // bristles (3 layers)
    pxRect(img, ox, oy, 8, 9, 15, 13, sf::Color(175, 135, 65));
    pxRect(img, ox, oy, 9, 11, 14, 14, sf::Color(155, 115, 50));
    pxRect(img, ox, oy, 10, 13, 13, 15, sf::Color(135, 95, 40));
    // bristle highlights
    for (int x = 9; x < 14; x += 2)
        pxRect(img, ox, oy, x, 10, x, 13, sf::Color(195, 155, 85));
}

// ========== school campus tiles ==========
static void drawSkyPx(sf::Image& img, int ox, int oy) {
    pxRect(img, ox, oy, 0, 0, 15, 15, sf::Color(120, 185, 235));
}
static void drawSeaPx(sf::Image& img, int ox, int oy) {
    pxRect(img, ox, oy, 0, 0, 15, 15, sf::Color(45, 115, 195));
    for (int y = 2; y < 16; y += 4) {
        pxRect(img, ox, oy, 1, y, 14, y, sf::Color(90, 160, 225));
        pxRect(img, ox, oy, 3, y, 13, y + 1, sf::Color(130, 195, 240));
    }
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (hash2(ox + x, oy + y) % 19 == 0)
                pxSet(img, ox, oy, x, y, sf::Color(70, 140, 215));
}
static void drawSandPx(sf::Image& img, int ox, int oy) {
    pxRect(img, ox, oy, 0, 0, 15, 15, sf::Color(222, 196, 130));
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (hash2(ox + x, oy + y) % 11 == 0)
                pxSet(img, ox, oy, x, y, sf::Color(240, 222, 170));
    pxRect(img, ox, oy, 0, 15, 15, 15, sf::Color(200, 172, 105));
}
static void drawGrassPx(sf::Image& img, int ox, int oy) {
    pxRect(img, ox, oy, 0, 0, 15, 15, sf::Color(95, 165, 70));
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (hash2(ox + x, oy + y) % 9 == 0)
                pxSet(img, ox, oy, x, y, sf::Color(120, 190, 85));
    pxRect(img, ox, oy, 0, 15, 15, 15, sf::Color(75, 135, 55));
}
static void drawRoadPx(sf::Image& img, int ox, int oy) {
    pxRect(img, ox, oy, 0, 0, 15, 15, sf::Color(150, 150, 150));
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (hash2(ox + x, oy + y) % 7 == 0)
                pxSet(img, ox, oy, x, y, sf::Color(168, 168, 168));
    // 车道中线（黄色虚线）
    pxRect(img, ox, oy, 0, 7, 15, 8, sf::Color(235, 200, 90));
    pxRect(img, ox, oy, 0, 7, 15, 7, sf::Color(250, 220, 130));
}
static void drawBrickPx(sf::Image& img, int ox, int oy) {
    pxRect(img, ox, oy, 0, 0, 15, 15, sf::Color(170, 75, 60));
    for (int y = 0; y < 16; y += 4) {
        int off = (y / 4) % 2 == 0 ? 0 : 4;
        pxRect(img, ox, oy, 0, y, 15, y, sf::Color(128, 52, 42));
        for (int x = off; x < 16; x += 8)
            pxRect(img, ox, oy, x, y, x, y + 3, sf::Color(128, 52, 42));
    }
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (hash2(ox + x, oy + y) % 13 == 0)
                pxSet(img, ox, oy, x, y, sf::Color(190, 95, 75));
}
static void drawSchoolWinPx(sf::Image& img, int ox, int oy) {
    drawBrickPx(img, ox, oy);
    pxRect(img, ox, oy, 3, 3, 12, 12, sf::Color(70, 100, 140));
    pxRect(img, ox, oy, 4, 4, 11, 11, sf::Color(130, 185, 230));
    pxRect(img, ox, oy, 4, 4, 11, 4, sf::Color(180, 215, 245));
    pxRect(img, ox, oy, 7, 4, 8, 11, sf::Color(70, 100, 140));
    pxRect(img, ox, oy, 4, 7, 11, 8, sf::Color(70, 100, 140));
}
static void drawSchoolBalconyPx(sf::Image& img, int ox, int oy) {
    drawBrickPx(img, ox, oy);
    pxRect(img, ox, oy, 0, 10, 15, 15, sf::Color(205, 205, 215));
    pxRect(img, ox, oy, 0, 10, 15, 10, sf::Color(235, 235, 240));
    pxRect(img, ox, oy, 0, 15, 15, 15, sf::Color(140, 140, 150));
    for (int x = 2; x < 16; x += 4)
        pxRect(img, ox, oy, x, 11, x + 1, 15, sf::Color(165, 165, 175));
}
static void drawTreePx(sf::Image& img, int ox, int oy) {
    // 树干
    pxRect(img, ox, oy, 6, 10, 9, 15, sf::Color(120, 80, 45));
    pxRect(img, ox, oy, 6, 10, 6, 15, sf::Color(140, 95, 55));
    // 树冠（多层圆）
    pxRect(img, ox, oy, 2, 3, 13, 11, sf::Color(45, 120, 60));
    pxRect(img, ox, oy, 3, 2, 12, 10, sf::Color(55, 140, 70));
    pxRect(img, ox, oy, 4, 1, 11, 8, sf::Color(75, 165, 85));
    pxRect(img, ox, oy, 5, 1, 7, 7, sf::Color(100, 190, 100));
}
static void drawFlowerPx(sf::Image& img, int ox, int oy) {
    drawGrassPx(img, ox, oy);
    auto dot = [&](int x, int y, sf::Color c) { pxRect(img, ox, oy, x, y, x + 1, y + 1, c); };
    dot(3, 4, sf::Color(235, 90, 95));
    dot(6, 9, sf::Color(245, 205, 80));
    dot(10, 3, sf::Color(230, 130, 190));
    dot(12, 8, sf::Color(245, 205, 80));
    dot(7, 12, sf::Color(235, 90, 95));
    dot(2, 11, sf::Color(255, 255, 255));
}

void drawTile(sf::Image& img, char c, int tx, int ty) {
    int ox = tx * TILE_RENDER, oy = ty * TILE_RENDER;
    switch (c) {
        case '#': drawWallPx(img, ox, oy); break;
        case '.': drawFloorPx(img, ox, oy); break;
        case 'o': drawFloorPx(img, ox, oy); break;
        case 'O': drawDoorPx(img, ox, oy); break;
        case 'G': drawGlassDoorPx(img, ox, oy); break;
        case 'W': drawWindowPx(img, ox, oy); break;
        case 'r': drawRailingPx(img, ox, oy); break;
        case 'Y': drawFloorPx(img, ox, oy); break;
        case 'B': case 'b': drawFloorPx(img, ox, oy); break;
        case 'D': {
            // 2-tile tall door: top half on row 2/7, bottom half on row 3/8
            bool isTop = (ty == 2 || ty == 7);
            drawHallDoorPx(img, ox, oy, isTop);
            break;
        }
        case 'H': {
            // dorm warp door: same 2-tile tall door look, walkable to trigger warp
            bool isTop = (ty == 7);
            drawHallDoorPx(img, ox, oy, isTop);
            break;
        }
        case 'E': drawBalconyDoorPx(img, ox, oy); break;
        case 'X': drawDoorPx(img, ox, oy); break;  // 各建筑房间出口门
        case 'M': {
            // campus door: top half on row 5 (hallway) / 7 (school)
            bool isTop = (ty == 5 || ty == 7);
            drawHallDoorPx(img, ox, oy, isTop);
            break;
        }
        // school campus
        case 'S': drawSkyPx(img, ox, oy); break;
        case '~': drawSeaPx(img, ox, oy); break;
        case 's': drawSandPx(img, ox, oy); break;
        case 'g': drawGrassPx(img, ox, oy); break;
        case 'P': case 'C':
            drawFloorPx(img, ox, oy);
            for (int y = 2; y < 14; ++y)
                for (int x = 2; x < 14; ++x) {
                    int dx = x - 8, dy = y - 8;
                    int d = dx*dx + dy*dy;
                    if (d <= 36 && d >= 16)
                        pxSet(img, ox, oy, x, y, sf::Color(120, 200, 255));
                    else if (d < 16)
                        pxSet(img, ox, oy, x, y, sf::Color(200, 235, 255));
                }
            break;
        case 'R': drawBrickPx(img, ox, oy); break;
        case 'w': drawSchoolWinPx(img, ox, oy); break;
        case 'a': drawSchoolBalconyPx(img, ox, oy); break;
        case 'T': drawTreePx(img, ox, oy); break;
        case 'F': drawFlowerPx(img, ox, oy); break;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            drawWallPx(img, ox, oy);
            drawDigitBig(img, ox, oy, c - '0');
            break;
        default: drawFloorPx(img, ox, oy); break;
    }
}

// ========== furniture (beds + broom) ==========
static sf::Image furnitureImg;
static bool furnitureLoaded = false;
static void ensureFurniture() {
    if (furnitureLoaded) return;
    furnitureLoaded = true;
    furnitureImg.loadFromFile("assets/furniture.png");
}

static void blitFurniture(sf::Image& img, int ox, int oy, int sx, int sy, int sw, int sh) {
    ensureFurniture();
    for (int y = 0; y < sh; ++y)
        for (int x = 0; x < sw; ++x) {
            sf::Color c = furnitureImg.getPixel(sx + x, sy + y);
            if (c.a > 10) pxSet(img, 0, 0, ox + x, oy + y, c);
        }
}

void drawFurniture(sf::Image& img) {
    const auto& m = getMap();
    int H = (int)m.size(), W = (int)m[0].size();

    // beds
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            if (m[y][x] == 'B') {
                bool isTopLeft = (y == 0 || m[y-1][x] != 'B') && (x == 0 || m[y][x-1] != 'B');
                if (isTopLeft) {
                    int px = x * TILE_RENDER, py = y * TILE_RENDER;
                    blitFurniture(img, px, py, 0, 1024, 32, 64);
                }
            }
        }
    }

    // broom 已由 NPC Quinn 替代（NPC 拿扫把在宿舍走动）
    // if (currentScene == SCENE_DORM && !broomPickedUp) {
    //     int bx = 8 * TILE_RENDER, by = 4 * TILE_RENDER;
    //     drawBroomPx(img, bx, by);
    // }
}
