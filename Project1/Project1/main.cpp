#define _CRT_SECURE_NO_WARNINGS
#include "Config.hpp"
#include "Map.hpp"
#include "Player.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

// 重建当前场景的地图纹理
static sf::Image mapImg;
static sf::Texture mapTex;
static sf::Sprite mapSprite;

static void drawBanners(sf::Image& img);  // 走廊横幅（定义见下）
static void drawPoster(sf::Image& img);   // 走廊海报（定义见下）

// 宿舍楼外景（学校场景）：俯视全景图 1:1 铺满整个场景（世界 32x28 格 = 2048x1792，与图等大），
// 加载失败时返回 false，由 rebuildMap 回退到原程序化地图
static bool drawDormExteriorMap() {
    static sf::Texture dormTex;
    static bool dormTried = false;
    if (!dormTried) {
        dormTried = true;
        if (!dormTex.loadFromFile("assets/dorm_building_topdown.png"))
            std::cerr << "无法加载宿舍楼外景图 assets/dorm_building_topdown.png，回退程序化地图" << std::endl;
        dormTex.setSmooth(false);
    }
    if (dormTex.getSize().x == 0) return false;

    const float mapPX = (float)(getMapW() * TILE);
    const float mapPY = (float)(getMapH() * TILE);
    float sc = std::max(mapPX / (float)dormTex.getSize().x,
                        mapPY / (float)dormTex.getSize().y);
    mapSprite.setTexture(dormTex, true);
    mapSprite.setScale(sc, sc);
    // 左上角对齐，整图完整铺满世界（世界尺寸已与图片等大，sc≈1）
    mapSprite.setOrigin(0.f, 0.f);
    mapSprite.setPosition(0.f, 0.f);
    return true;
}

// 校园大地图：整图铺到 mapImg，再在传送点画门垫
static bool drawCampusMap() {
    static sf::Image campusImg;
    static bool campusTried = false;
    if (!campusTried) {
        campusTried = true;
        if (!campusImg.loadFromFile("assets/campus_map.png"))
            std::cerr << "无法加载校园地图 assets/campus_map.png，回退程序化地图" << std::endl;
    }
    if (campusImg.getSize().x == 0) return false;

    const int W = getMapW(), H = getMapH();
    mapImg.create(W * TILE_RENDER, H * TILE_RENDER);
    // 把校园图缩绘到 mapImg
    float sx = (float)campusImg.getSize().x / (W * TILE_RENDER);
    float sy = (float)campusImg.getSize().y / (H * TILE_RENDER);
    for (int y = 0; y < H * TILE_RENDER; ++y)
        for (int x = 0; x < W * TILE_RENDER; ++x) {
            int ix = (int)(x * sx), iy = (int)(y * sy);
            mapImg.setPixel(x, y, campusImg.getPixel(ix, iy));
        }
    // 在传送点格子画门垫
    auto pad = [&](int tx, int ty, sf::Color ring, sf::Color fill) {
        int ox = tx * TILE_RENDER, oy = ty * TILE_RENDER;
        for (int y = 2; y < TILE_RENDER - 2; ++y)
            for (int x = 2; x < TILE_RENDER - 2; ++x) {
                int dx = x - TILE_RENDER/2, dy = y - TILE_RENDER/2;
                int d = dx*dx + dy*dy;
                if (d <= 36 && d >= 16) mapImg.setPixel(ox+x, oy+y, ring);
                else if (d < 16) mapImg.setPixel(ox+x, oy+y, fill);
            }
    };
    sf::Color yRing(245,205,85), yFill(255,240,180);
    sf::Color bRing(120,200,255), bFill(200,235,255);
    int yellow[][2] = {{4,7},{3,15},{4,14},{25,7},{24,22},{14,20}};
    for (auto& p : yellow) pad(p[0], p[1], yRing, yFill);
    pad(3,10, bRing, bFill);
    pad(0,14, bRing, bFill);

    mapTex.loadFromImage(mapImg);
    mapTex.setSmooth(false);
    mapSprite.setTexture(mapTex, true);
    mapSprite.setOrigin(0.f, 0.f);
    mapSprite.setPosition(0.f, 0.f);
    mapSprite.setScale(TILE / (float)TILE_RENDER, TILE / (float)TILE_RENDER);
    return true;
}

static void rebuildMap() {
    const int W = getMapW();
    const int H = getMapH();
    mapImg.create(W * TILE_RENDER, H * TILE_RENDER);

    // 宿舍楼外（学校场景）：铺生成图；失败则退回下方程序化地图
    if (getCurrentScene() == SCENE_SCHOOL && drawDormExteriorMap())
        return;
    // 校园大地图：整图铺底
    if (getCurrentScene() == SCENE_CAMPUS && drawCampusMap())
        return;

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            drawTile(mapImg, getMap()[y][x], x, y);
    drawFurniture(mapImg);
    drawBanners(mapImg);
    drawPoster(mapImg);
    mapTex.loadFromImage(mapImg);
    mapTex.setSmooth(false);
    mapSprite.setTexture(mapTex, true);
    mapSprite.setOrigin(0.f, 0.f);
    mapSprite.setPosition(0.f, 0.f);
    mapSprite.setScale(TILE / (float)TILE_RENDER, TILE / (float)TILE_RENDER);
}

// ============ dialogue & stats ============
static sf::Font dialogFont;
static bool fontLoaded = false;
static int stamina = 5;
static int cleanliness = 0;
static int lazy = 0;          // 懒惰
static int askType = 0;       // 0=和金椰扫地 1=床上睡觉
static bool ending = false;   // 触发结局黑屏
static int endingKind = 0;    // 0=懒惰结局 1=结算结局
static sf::Clock endingClock; // 结局开始计时（3 秒后出现确认键）

enum class Dlg { None, Ask, Result, Poster };
static Dlg dlg = Dlg::None;
static int dlgChoice = 0;      // 0=是 1=否
static std::string dlgResult;  // 结果文本（UTF-8）

// ============ hallway poster (墙上小海报 + 交互全屏) ============
static sf::Texture posterTex;
static bool posterTried = false;

static sf::String U8(const char* s) {
    return sf::String::fromUtf8(s, s + std::strlen(s));
}

// ============ sprint double-tap ============
static sf::Clock tapClock;
static float lastTapTime[4] = { -9.f, -9.f, -9.f, -9.f };  // 0下 1上 2右 3左

// ============ hallway banner (当兵横幅，贴在走廊头部) ============
static void drawBanners(sf::Image& img) {
    if (getCurrentScene() != SCENE_HALLWAY) return;
    if (!fontLoaded) {
        const char* cands[] = {
            "C:/Windows/Fonts/simhei.ttf",
            "C:/Windows/Fonts/msyh.ttc",
            "C:/Windows/Fonts/simsun.ttc",
        };
        for (const char* f : cands)
            if (dialogFont.loadFromFile(f)) { fontLoaded = true; break; }
    }
    if (!fontLoaded) return;

    // 只有一条横幅："好儿女，当兵去！"，贴在走廊头部墙体中间（y=2，x=1..15），字号不变
    const char* text = u8"好儿女，当兵去！";
    const int bx0 = 1, bx1 = 15, by = 2;
    const int bh = TILE_RENDER;
    const int bw = (bx1 - bx0 + 1) * TILE_RENDER;

    sf::RenderTexture rt;
    if (!rt.create(bw, bh)) return;
    rt.clear(sf::Color::Transparent);
    // 红底
    sf::RectangleShape bg(sf::Vector2f((float)bw, (float)bh));
    bg.setFillColor(sf::Color(190, 35, 35));
    rt.draw(bg);
    // 金边
    sf::RectangleShape edge(sf::Vector2f(bw - 2.f, bh - 2.f));
    edge.setPosition(1.f, 1.f);
    edge.setFillColor(sf::Color::Transparent);
    edge.setOutlineColor(sf::Color(255, 220, 120));
    edge.setOutlineThickness(2.f);
    rt.draw(edge);
    // 文字（字号明显小于横幅高度，完整显示）
    sf::Text txt;
    txt.setFont(dialogFont);
    txt.setCharacterSize(22);
    txt.setString(U8(text));
    txt.setFillColor(sf::Color(255, 255, 230));
    txt.setOutlineColor(sf::Color(40, 20, 20));
    txt.setOutlineThickness(1.f);
    sf::FloatRect tb = txt.getLocalBounds();
    txt.setOrigin(tb.left + tb.width * 0.5f, tb.top + tb.height * 0.5f);
    txt.setPosition(bw * 0.5f, bh * 0.5f - 1.f);
    rt.draw(txt);
    rt.display();

    sf::Image bi = rt.getTexture().copyToImage();
    const int ox = bx0 * TILE_RENDER, oy = by * TILE_RENDER;
    for (int yy = 0; yy < bh && oy + yy < (int)img.getSize().y; ++yy)
        for (int xx = 0; xx < bw; ++xx) {
            sf::Color c = bi.getPixel(xx, yy);
            if (c.a > 10) img.setPixel(ox + xx, oy + yy, c);
        }
}

// ============ hallway poster (参考图贴在墙上) ============
static void drawPoster(sf::Image& img) {
    if (getCurrentScene() != SCENE_HALLWAY) return;
    if (!posterTried) {
        posterTried = true;
        if (!posterTex.loadFromFile("assets/school_poster.png"))
            std::cerr << "无法加载海报图片 assets/school_poster.png" << std::endl;
    }
    if (posterTex.getSize().x == 0 || posterTex.getSize().y == 0) return;

    // 墙上小海报（差不多的人影）：走廊尾部 x=16..17, y=2..4
    const int ox = 16 * TILE_RENDER, oy = 2 * TILE_RENDER;
    const int pw = 2 * TILE_RENDER, ph = 3 * TILE_RENDER;

    sf::RenderTexture rt;
    if (!rt.create(pw, ph)) return;
    rt.clear(sf::Color(232, 226, 210));  // 米色纸底

    // contain 缩放完整显示（不裁切）
    sf::Sprite sp(posterTex);
    float sx = (float)pw / (float)posterTex.getSize().x;
    float sy = (float)ph / (float)posterTex.getSize().y;
    float sc = (sx < sy) ? sx : sy;
    sp.setScale(sc, sc);
    sp.setOrigin((float)posterTex.getSize().x * 0.5f, (float)posterTex.getSize().y * 0.5f);
    sp.setPosition((float)pw * 0.5f, (float)ph * 0.5f);
    rt.draw(sp);

    // 白色画框
    sf::RectangleShape frame(sf::Vector2f(pw - 4.f, ph - 4.f));
    frame.setPosition(2.f, 2.f);
    frame.setFillColor(sf::Color::Transparent);
    frame.setOutlineColor(sf::Color(235, 230, 210));
    frame.setOutlineThickness(2.f);
    rt.draw(frame);
    rt.display();

    sf::Image pi = rt.getTexture().copyToImage();
    for (int yy = 0; yy < ph && oy + yy < (int)img.getSize().y; ++yy)
        for (int xx = 0; xx < pw; ++xx) {
            sf::Color c = pi.getPixel(xx, yy);
            if (c.a > 10) img.setPixel(ox + xx, oy + yy, c);
        }
}

// ============ NPC 金椰（在宿舍慢走，替代扫帚交互） ============
struct NpcQuinn {
    sf::Texture tex;
    sf::Sprite spr;
    bool loaded = false;
    sf::Vector2f pos;      // 底部中心（世界坐标）
    int dir = 0;           // 0下 1上 2右 3左
    int frame = 0;
    float animTimer = 0.f;
    int animStep = 0;
    int moveDir = -1;      // -1 站立，否则行走方向
    float moveTimer = 0.f; // 行走剩余时间
    float pauseTimer = 1.f;// 站立剩余时间

    bool loadNpc() {
        if (loaded) return true;
        if (!tex.loadFromFile("assets/quinn.png")) return false;
        tex.setSmooth(false);
        spr.setTexture(tex);
        spr.setOrigin(8.f, 32.f);
        spr.setScale(4.f, 4.f);
        loaded = true;
        return true;
    }

    sf::FloatRect hitbox() const {
        const float w = TILE * 0.62f;
        const float h = TILE * 0.38f;
        return sf::FloatRect(pos.x - w * 0.5f, pos.y - h, w, h);
    }

    void update(float dt, const std::function<bool(const sf::FloatRect&)>& collideFn) {
        if (!loaded) return;
        const float SPEED = 70.f;   // 不快

        if (moveDir >= 0) {
            moveTimer -= dt;
            sf::Vector2f d(0.f, 0.f);
            if (moveDir == 0) d.y = 1.f; else if (moveDir == 1) d.y = -1.f;
            else if (moveDir == 2) d.x = 1.f; else if (moveDir == 3) d.x = -1.f;
            const sf::Vector2f step = d * SPEED * dt;
            bool blocked = false;
            if (step.x != 0.f) { float old = pos.x; pos.x += step.x; if (collideFn(hitbox())) { pos.x = old; blocked = true; } }
            if (step.y != 0.f) { float old = pos.y; pos.y += step.y; if (collideFn(hitbox())) { pos.y = old; blocked = true; } }
            dir = moveDir;
            // 走路动画
            animTimer += dt;
            static const int walkSeq[4] = { 0, 1, 2, 1 };
            while (animTimer >= 0.22f) { animTimer -= 0.22f; animStep = (animStep + 1) % 4; }
            frame = walkSeq[animStep];
            if (blocked || moveTimer <= 0.f) {
                moveDir = -1;
                pauseTimer = 0.8f + 1.6f * std::fmod(tapClock.getElapsedTime().asSeconds() * 1.3f, 1.f);
            }
        } else {
            pauseTimer -= dt;
            frame = 0;
            if (pauseTimer <= 0.f) {
                moveDir = (int)(tapClock.getElapsedTime().asSeconds() * 7.13f) % 4;
                moveTimer = 1.2f + 0.8f * std::fmod(tapClock.getElapsedTime().asSeconds() * 0.7f, 1.f);
            }
        }
    }

    void draw(sf::RenderTarget& rt, const sf::Vector2f&) {
        if (!loaded) return;
        spr.setTextureRect(sf::IntRect(frame * 16, dir * 32, 16, 32));
        spr.setPosition(std::round(pos.x), std::round(pos.y));
        rt.draw(spr);
    }
};

int main() {
    sf::RenderWindow window(
        sf::VideoMode(WIN_W, WIN_H),
        "Pixel RPG - Sophia",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    rebuildMap();

    Player player;
    if (!player.load(SPRITE_PATH)) {
        std::cerr << "无法加载角色贴图：\n" << SPRITE_PATH << std::endl;
    }
    player.pos.x = (float)(SPAWN_TX * TILE + TILE / 2);
    player.pos.y = (float)(SPAWN_TY * TILE + TILE / 2);

    // NPC 金椰（在宿舍走动，初始在原扫帚位置）
    NpcQuinn npc;
    if (!npc.loadNpc())
        std::cerr << "无法加载 NPC 贴图 assets/quinn.png" << std::endl;
    npc.pos.x = (float)(8 * TILE + TILE / 2);
    npc.pos.y = (float)(5 * TILE);

    auto collide = [&](const sf::FloatRect& r) -> bool {
        const int W = getMapW();
        const int H = getMapH();
        const int x0 = (int)std::floor(r.left / TILE);
        const int y0 = (int)std::floor(r.top / TILE);
        const int x1 = (int)std::floor((r.left + r.width - 1.f) / TILE);
        const int y1 = (int)std::floor((r.top + r.height - 1.f) / TILE);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                if (x < 0 || y < 0 || x >= W || y >= H)
                    return true;
                if (isSolidTile(getMap()[y][x]))
                    return true;
            }
        return false;
    };

    sf::View view(sf::FloatRect(0.f, 0.f, (float)WIN_W, (float)WIN_H));
    bool showDebug = false;
    sf::Clock clock;
    bool warpCooldown = false; // 防止传送后立即触发反向传送

    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed)
                window.close();
            if (e.type == sf::Event::KeyPressed) {
                bool dlgOpenedThisEvent = false;   // 本次按键是否刚打开对话框（防 E 立即确认）
                if (e.key.code == sf::Keyboard::Escape)
                    window.close();
                if (e.key.code == sf::Keyboard::F1)
                    showDebug = !showDebug;
                // 结局黑屏：3 秒后按 E 继续（同时开启新回合，属性重置 + 人物回出生点）
                if (ending && endingClock.getElapsedTime().asSeconds() >= 3.f
                    && (e.key.code == sf::Keyboard::E
                    || e.key.code == sf::Keyboard::Enter
                    || e.key.code == sf::Keyboard::Space)) {
                    ending = false;
                    stamina = 5;
                    cleanliness = 0;
                    lazy = 0;
                    dlg = Dlg::None;
                    dlgChoice = 0;
                    askType = 0;
                    setScene(SCENE_DORM);
                    player.pos.x = (float)(SPAWN_TX * TILE + TILE / 2);
                    player.pos.y = (float)(SPAWN_TY * TILE + TILE / 2);
                    player.sprint = false;
                    player.sprintDir = -1;
                }
                if (e.key.code == sf::Keyboard::E) {
                    if (dlg == Dlg::None) {
                        int ptx = (int)std::floor(player.pos.x / TILE);
                        int pty = (int)std::floor(player.pos.y / TILE);
                        // 1) 当兵横幅交互（走廊头部）：索菲亚对"好儿女，当兵去！"的积极评价
                        if (getCurrentScene() == SCENE_HALLWAY && pty == 5 && ptx >= 1 && ptx <= 15) {
                            static const char* evals[3] = {
                                u8"索菲亚：好男儿志在四方，参军报国无上光荣！",
                                u8"索菲亚：热血青春献国防，向每一位军人致敬！",
                                u8"索菲亚：保卫祖国人人有责，我也想成为那样有担当的人！",
                            };
                            int r = ((int)(tapClock.getElapsedTime().asSeconds() * 7.13f)) % 3;
                            dlg = Dlg::Result;
                            dlgResult = evals[r];
                        }
                        // 2) 海报交互（走廊尾部）：全屏完整海报 + 台词框
                        //    （对话框在镜头下方固定位置，玩家无需移动即可完整看到）
                        if (dlg == Dlg::None && getCurrentScene() == SCENE_HALLWAY
                            && posterTex.getSize().x > 0
                            && pty == 5 && ptx >= 16 && ptx <= 17) {
                            dlg = Dlg::Poster;
                        }
                        // 3) NPC 金椰交互（在宿舍走动）
                        if (dlg == Dlg::None && getCurrentScene() == SCENE_DORM && npc.loaded) {
                            float dx = npc.pos.x - player.pos.x;
                            float dy = npc.pos.y - player.pos.y;
                            if (dx * dx + dy * dy < 100.f * 100.f) {
                                dlg = Dlg::Ask;
                                askType = 0;   // 扫地对话
                                dlgChoice = 0;
                                dlgResult.clear();
                                dlgOpenedThisEvent = true;
                            }
                        }
                        // 4) 床交互（只有左上角床位的床头旁可触发：站 (3,1)）
                        if (dlg == Dlg::None && getCurrentScene() == SCENE_DORM
                            && ptx == 3 && pty == 1) {
                            dlg = Dlg::Ask;
                            askType = 1;   // 睡觉对话
                            dlgChoice = 0;
                            dlgResult.clear();
                            dlgOpenedThisEvent = true;
                        }
                    } else if (dlg == Dlg::Result || dlg == Dlg::Poster) {
                        dlg = Dlg::None;   // 读完结果/看完海报关闭
                    }
                }
                // dialogue input
                if (dlg == Dlg::Ask) {
                    if (e.key.code == sf::Keyboard::W || e.key.code == sf::Keyboard::Up)
                        dlgChoice = 0;
                    else if (e.key.code == sf::Keyboard::S || e.key.code == sf::Keyboard::Down)
                        dlgChoice = 1;
                    else if (!dlgOpenedThisEvent && e.key.code == sf::Keyboard::E) {
                        if (dlgChoice == 0) {
                            if (stamina > 0) {
                                --stamina;
                                if (askType == 0) {   // 和金椰一起扫地
                                    ++cleanliness;
                                    dlgResult = u8"消耗一点行动点，宿舍整洁度+1";
                                    if (cleanliness >= 5) {   // 整洁度=5 触发整洁结局
                                        ending = true;
                                        endingKind = 2;
                                        endingClock.restart();
                                        dlg = Dlg::None;
                                    } else if (stamina <= 0) {   // 行动点耗尽 → 结算结局
                                        ending = true;
                                        endingKind = 1;
                                        endingClock.restart();
                                        dlg = Dlg::None;
                                    }
                                } else {              // 睡觉
                                    ++lazy;
                                    dlgResult = u8"睡了一觉，行动点-1，懒惰+1";
                                    if (lazy >= 5) {   // 懒惰=5 触发懒惰结局
                                        ending = true;
                                        endingKind = 0;
                                        endingClock.restart();
                                        dlg = Dlg::None;
                                    } else if (stamina <= 0) {   // 行动点耗尽 → 结算结局
                                        ending = true;
                                        endingKind = 1;
                                        endingClock.restart();
                                        dlg = Dlg::None;
                                    }
                                }
                            } else {
                                dlgResult = u8"行动点不足，先休息一下吧！";
                            }
                            dlg = Dlg::Result;
                        } else {
                            dlg = Dlg::None;   // 选"否"，关闭
                        }
                    }
                }
                // sprint double-tap detection (0下 1上 2右 3左)
                {
                    int td = -1;
                    if (e.key.code == sf::Keyboard::S || e.key.code == sf::Keyboard::Down) td = 0;
                    else if (e.key.code == sf::Keyboard::W || e.key.code == sf::Keyboard::Up) td = 1;
                    else if (e.key.code == sf::Keyboard::D || e.key.code == sf::Keyboard::Right) td = 2;
                    else if (e.key.code == sf::Keyboard::A || e.key.code == sf::Keyboard::Left) td = 3;
                    if (td >= 0) {
                        float now = tapClock.getElapsedTime().asSeconds();
                        if (now - lastTapTime[td] < DOUBLE_TAP_WINDOW) {
                            player.sprint = true;
                            player.sprintDir = td;
                        }
                        lastTapTime[td] = now;
                    }
                }
            }
            // 松开方向键解除对应方向的冲刺
            if (e.type == sf::Event::KeyReleased) {
                int rd = -1;
                if (e.key.code == sf::Keyboard::S || e.key.code == sf::Keyboard::Down) rd = 0;
                else if (e.key.code == sf::Keyboard::W || e.key.code == sf::Keyboard::Up) rd = 1;
                else if (e.key.code == sf::Keyboard::D || e.key.code == sf::Keyboard::Right) rd = 2;
                else if (e.key.code == sf::Keyboard::A || e.key.code == sf::Keyboard::Left) rd = 3;
                if (rd >= 0 && player.sprint && player.sprintDir == rd)
                    player.sprint = false;
            }
        }

        float dt = clock.restart().asSeconds();
        if (dt > 0.1f) dt = 0.1f;

        player.update(dt, collide);
        if (getCurrentScene() == SCENE_DORM)
            npc.update(dt, collide);

        // 检测传送点
        int ptx = (int)std::floor(player.pos.x / TILE);
        int pty = (int)std::floor(player.pos.y / TILE);
        int target = checkWarp(ptx, pty);
        if (target >= 0 && !warpCooldown) {
            dlg = Dlg::None;  // 传送时关闭对话
            int sx, sy;
            getWarpSpawn(getCurrentScene(), target, sx, sy);
            setScene(target);
            rebuildMap();
            player.pos.x = (float)(sx * TILE + TILE / 2);
            player.pos.y = (float)(sy * TILE + TILE / 2);
            warpCooldown = true;
        } else if (target < 0) {
            warpCooldown = false;
        }

        // 相机
        const float mapPX = (float)(getMapW() * TILE);
        const float mapPY = (float)(getMapH() * TILE);
        const float halfW = WIN_W * 0.5f;
        const float halfH = WIN_H * 0.5f;
        float camX, camY;
        if (mapPX <= WIN_W) camX = mapPX * 0.5f;
        else camX = std::max(halfW, std::min(player.pos.x, mapPX - halfW));
        if (mapPY <= WIN_H) camY = mapPY * 0.5f;
        else camY = std::max(halfH, std::min(player.pos.y, mapPY - halfH));
        view.setCenter(std::round(camX), std::round(camY));
        window.setView(view);

        // 背景色（阳台/学校用天空蓝，其他用深色）
        if (getCurrentScene() == SCENE_BALCONY || getCurrentScene() == SCENE_SCHOOL)
            window.clear(sf::Color(100, 160, 210));
        else
            window.clear(sf::Color(18, 24, 18));

        window.draw(mapSprite);
        if (getCurrentScene() == SCENE_DORM)
            npc.draw(window, view.getCenter());
        player.draw(window);

        // ---------- 交互位置提示光标（画在每个可交互点上，闪烁 E） ----------
        if (dlg == Dlg::None) {
            float blink = 0.5f + 0.5f * std::sin(tapClock.getElapsedTime().asSeconds() * 6.0f);
            sf::Uint8 a = (sf::Uint8)(165 + 90 * blink);   // 亮度下限提高，保证始终可见
            auto hintAt = [&](float wx, float wy) {
                const float sx = wx - (view.getCenter().x - WIN_W * 0.5f);
                const float sy = wy - (view.getCenter().y - WIN_H * 0.5f);
                // 闪烁倒三角（尖端朝下）
                sf::CircleShape badge(16.f, 3);
                badge.setRotation(270.f);
                badge.setFillColor(sf::Color(245, 205, 85, a));
                badge.setOutlineColor(sf::Color(55, 35, 10, a));
                badge.setOutlineThickness(2.f);
                badge.setPosition(sx - 16.f, sy - 16.f);
                window.draw(badge);
                if (fontLoaded) {
                    sf::Text eTxt;
                    eTxt.setFont(dialogFont);
                    eTxt.setCharacterSize(14);
                    eTxt.setFillColor(sf::Color(45, 28, 5, a));
                    eTxt.setString("E");
                    sf::FloatRect eb = eTxt.getLocalBounds();
                    eTxt.setOrigin(eb.left + eb.width * 0.5f, eb.top + eb.height * 0.5f);
                    eTxt.setPosition(sx, sy - 26.f);
                    window.draw(eTxt);
                }
            };
            if (getCurrentScene() == SCENE_HALLWAY) {
                // 当兵横幅（走廊头部，光标固定在横幅墙面中间）
                hintAt(8.f * TILE + 32.f, 2.f * TILE + 8.f);
                // 海报（走廊尾部，光标固定在海报中下部）
                if (posterTex.getSize().x > 0)
                    hintAt(17.f * TILE, 4.f * TILE + 32.f);
            }
            if (getCurrentScene() == SCENE_DORM) {
                // NPC 金椰（头顶）
                if (npc.loaded)
                    hintAt(npc.pos.x, npc.pos.y - 132.f);
                // 左上角床床头旁
                hintAt(3.f * TILE + 32.f, 1.f * TILE + 36.f);
            }
        }

        // ---------- dialogue box ----------
        if (dlg != Dlg::None) {
            // UI 固定在玩家镜头上（屏幕坐标，追踪玩家，交互后直接可见）
            window.setView(window.getDefaultView());
            if (!fontLoaded) {
                const char* cands[] = {
                    "C:/Windows/Fonts/simhei.ttf",
                    "C:/Windows/Fonts/msyh.ttc",
                    "C:/Windows/Fonts/simsun.ttc",
                };
                for (const char* f : cands)
                    if (dialogFont.loadFromFile(f)) { fontLoaded = true; break; }
            }
            if (dlg != Dlg::Poster) {
                sf::RectangleShape box(sf::Vector2f(WIN_W - 100.f, 140.f));
                box.setPosition(50.f, WIN_H - 170.f);
                box.setFillColor(sf::Color(10, 14, 10, 215));
                box.setOutlineColor(sf::Color(205, 180, 130));
                box.setOutlineThickness(2.f);
                window.draw(box);
            }

            if (fontLoaded) {
                if (dlg == Dlg::Ask) {
                    sf::Text t;
                    t.setFont(dialogFont);
                    t.setCharacterSize(24);
                    t.setFillColor(sf::Color(240, 230, 200));
                    t.setString(U8(askType == 0
                        ? u8"金椰：啊我，我在扫地呀，你要来和我一起扫地吗（笑）"
                        : u8"是否消耗一点行动点睡觉"));
                    t.setPosition(70.f, WIN_H - 152.f);
                    window.draw(t);

                    sf::Text opt[2];
                    for (int i = 0; i < 2; ++i) {
                        opt[i].setFont(dialogFont);
                        opt[i].setCharacterSize(22);
                        opt[i].setString(U8(i == 0 ? u8"是" : u8"否"));
                        opt[i].setFillColor(dlgChoice == i ? sf::Color(255, 215, 90) : sf::Color(185, 175, 155));
                        opt[i].setPosition(70.f + i * 90.f, WIN_H - 108.f);
                        if (dlgChoice == i)
                            opt[i].setString(U8(i == 0 ? u8"▶ 是" : u8"▶ 否"));
                        window.draw(opt[i]);
                    }
                    sf::Text hint;
                    hint.setFont(dialogFont);
                    hint.setCharacterSize(14);
                    hint.setFillColor(sf::Color(150, 140, 120));
                    hint.setString(U8(u8"↑/↓ 选择　E 确认"));
                    hint.setPosition(70.f, WIN_H - 68.f);
                    window.draw(hint);
                } else if (dlg == Dlg::Result) {
                    sf::Text t;
                    t.setFont(dialogFont);
                    t.setCharacterSize(22);
                    t.setFillColor(sf::Color(240, 230, 200));
                    t.setString(U8(dlgResult.c_str()));
                    t.setPosition(70.f, WIN_H - 120.f);
                    window.draw(t);
                    sf::Text hint;
                    hint.setFont(dialogFont);
                    hint.setCharacterSize(14);
                    hint.setFillColor(sf::Color(150, 140, 120));
                    hint.setString(U8(u8"按 E 关闭"));
                    hint.setPosition(70.f, WIN_H - 80.f);
                    window.draw(hint);
                } else {  // Dlg::Poster：海报交互，全屏展示完整海报 + 镜头下方台词（无需移动即可看全）
                    sf::RectangleShape dim(sf::Vector2f((float)WIN_W, (float)WIN_H));
                    dim.setFillColor(sf::Color(0, 0, 0, 185));
                    window.draw(dim);
                    if (posterTex.getSize().x > 0) {
                        sf::Sprite p(posterTex);
                        const float px = (float)posterTex.getSize().x;
                        const float py = (float)posterTex.getSize().y;
                        const float availW = WIN_W - 60.f;
                        const float availH = WIN_H - 170.f;  // 给底部台词留位置
                        float sc = (availW / px < availH / py) ? availW / px : availH / py;
                        p.setScale(sc, sc);
                        p.setOrigin(px * 0.5f, py * 0.5f);
                        // 玩家镜头正中间
                        p.setPosition(WIN_W * 0.5f, (WIN_H - 170.f) * 0.5f + 25.f);
                        window.draw(p);
                    }
                    // 底部对话框（镜头下方固定位置）
                    sf::RectangleShape box(sf::Vector2f(WIN_W - 100.f, 120.f));
                    box.setPosition(50.f, WIN_H - 145.f);
                    box.setFillColor(sf::Color(10, 14, 10, 225));
                    box.setOutlineColor(sf::Color(205, 180, 130));
                    box.setOutlineThickness(2.f);
                    window.draw(box);
                    sf::Text t;
                    t.setFont(dialogFont);
                    t.setCharacterSize(24);
                    t.setFillColor(sf::Color(240, 230, 200));
                    t.setString(U8(u8"索菲亚：这个就是当兵的楷模学长，震陶吧"));
                    // 若台词过宽则缩小字号，保证玩家无需移动即可完整看到
                    sf::FloatRect tb = t.getLocalBounds();
                    if (tb.width > WIN_W - 140.f) t.setCharacterSize(20);
                    t.setPosition(70.f, WIN_H - 128.f);
                    window.draw(t);
                    sf::Text hint;
                    hint.setFont(dialogFont);
                    hint.setCharacterSize(14);
                    hint.setFillColor(sf::Color(150, 140, 120));
                    hint.setString(U8(u8"按 E 关闭"));
                    hint.setPosition(70.f, WIN_H - 92.f);
                    window.draw(hint);
                }
            }

            // stats panel（查看海报时不显示）
            if (fontLoaded && dlg != Dlg::Poster) {
                std::string statsUtf8 = u8"行动点：" + std::to_string(stamina)
                    + u8"　宿舍整洁度：" + std::to_string(cleanliness)
                    + u8"　懒惰：" + std::to_string(lazy);
                sf::Text stats;
                stats.setFont(dialogFont);
                stats.setCharacterSize(16);
                stats.setFillColor(sf::Color(175, 205, 175));
                stats.setString(U8(statsUtf8.c_str()));
                stats.setPosition(70.f, WIN_H - 44.f);
                window.draw(stats);
            }
        }

        if (showDebug) {
            window.setView(view);  // 恢复世界坐标（debug 框用世界坐标）
            sf::FloatRect b = player.debugBox();
            sf::RectangleShape r(sf::Vector2f(b.width, b.height));
            r.setPosition(b.left, b.top);
            r.setFillColor(sf::Color(255, 0, 0, 60));
            r.setOutlineColor(sf::Color::Red);
            r.setOutlineThickness(1.f);
            window.draw(r);
        }

        // ---------- 结局：黑屏 + 对话框（3 秒后出现确认键） ----------
        if (ending) {
            window.setView(window.getDefaultView());
            sf::RectangleShape black(sf::Vector2f((float)WIN_W, (float)WIN_H));
            black.setFillColor(sf::Color(0, 0, 0));
            window.draw(black);
            if (fontLoaded) {
                sf::RectangleShape box(sf::Vector2f(WIN_W - 100.f, 160.f));
                box.setPosition(50.f, WIN_H - 200.f);
                box.setFillColor(sf::Color(10, 14, 10, 235));
                box.setOutlineColor(sf::Color(205, 180, 130));
                box.setOutlineThickness(2.f);
                window.draw(box);
                sf::Text title;
                title.setFont(dialogFont);
                title.setCharacterSize(30);
                title.setFillColor(sf::Color(255, 215, 90));
                title.setPosition(70.f, WIN_H - 185.f);
                if (endingKind == 0) {   // 懒惰结局
                    title.setString(U8(u8"解锁结局"));
                    window.draw(title);
                    sf::Text line;
                    line.setFont(dialogFont);
                    line.setCharacterSize(22);
                    line.setFillColor(sf::Color(240, 230, 200));
                    line.setString(U8(u8"只好回家继承百万家产55555"));
                    line.setPosition(70.f, WIN_H - 140.f);
                    window.draw(line);
                } else if (endingKind == 2) {         // 整洁结局（整洁度=5）
                    title.setString(U8(u8"解锁结局"));
                    window.draw(title);
                    sf::Text line1;
                    line1.setFont(dialogFont);
                    line1.setCharacterSize(22);
                    line1.setFillColor(sf::Color(240, 230, 200));
                    line1.setString(U8(u8"金椰：大家都爱干净"));
                    line1.setPosition(70.f, WIN_H - 140.f);
                    window.draw(line1);
                    sf::Text line2;
                    line2.setFont(dialogFont);
                    line2.setCharacterSize(22);
                    line2.setFillColor(sf::Color(240, 230, 200));
                    line2.setString(U8(u8"宿舍是我家，文明宿舍怎么能少了我捏！"));
                    line2.setPosition(70.f, WIN_H - 110.f);
                    window.draw(line2);
                } else {                 // 结算结局（行动点耗尽）
                    title.setString(U8(u8"结算结局"));
                    window.draw(title);
                    sf::Text line;
                    line.setFont(dialogFont);
                    line.setCharacterSize(22);
                    line.setFillColor(sf::Color(240, 230, 200));
                    line.setString(U8(u8"行动点耗尽，今日到此为止！"));
                    line.setPosition(70.f, WIN_H - 140.f);
                    window.draw(line);
                    sf::Text stats;
                    stats.setFont(dialogFont);
                    stats.setCharacterSize(18);
                    stats.setFillColor(sf::Color(175, 205, 175));
                    std::string s = u8"宿舍整洁度：" + std::to_string(cleanliness)
                        + u8"　懒惰：" + std::to_string(lazy);
                    stats.setString(U8(s.c_str()));
                    stats.setPosition(70.f, WIN_H - 112.f);
                    window.draw(stats);
                }
                if (endingClock.getElapsedTime().asSeconds() >= 3.f) {
                    sf::Text tip;
                    tip.setFont(dialogFont);
                    tip.setCharacterSize(14);
                    tip.setFillColor(sf::Color(150, 140, 120));
                    tip.setString(U8(u8"按 E 继续"));
                    tip.setPosition(70.f, WIN_H - 74.f);
                    window.draw(tip);
                }
            }
        }

        window.display();
    }
    return 0;
}
