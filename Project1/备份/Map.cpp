#include "Map.hpp"
#include "Config.hpp"

// ============================================================
//  内部辅助函数
// ============================================================
static float hash2(int x, int y, int seed) {
    unsigned int n = (unsigned int)x * 374761393u
        + (unsigned int)y * 668265263u
        + (unsigned int)seed * 1442695041u;
    n = (n ^ (n >> 13)) * 1274126177u;
    n ^= (n >> 16);
    return (float)(n & 0xFFFFu) / 65535.0f;
}

static int clamp255(int v) {
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

static void baseTile(sf::RenderTarget& rt, int tx, int ty,
    sf::Color col, int variation, int seed = 7) {
    const float v = hash2(tx, ty, seed);
    const int d = (int)((v - 0.5f) * variation);
    sf::RectangleShape r(sf::Vector2f((float)TILE, (float)TILE));
    r.setPosition((float)(tx * TILE), (float)(ty * TILE));
    r.setFillColor(sf::Color(clamp255(col.r + d),
        clamp255(col.g + d),
        clamp255(col.b + d)));
    rt.draw(r);
}

// ============================================================
//  地图数据
//  15 x 10 格子
//  # = 墙       . = 木地板
//  O = 宿舍门   G = 阳台玻璃门
//  o = 阳台地面  r = 栏杆
//  Y = 扫把
//  B/b = 床（左半/右半）  D/E = 北侧书桌（左/右）  d/e = 南侧书桌（左/右）
// ============================================================
const std::vector<std::string> MAP = {
    "###############",   // y = 0
    "#.Bb.Bb.Bb.###",   // y = 1  北侧床
    "#.DE.DE.DE.#rrr",   // y = 2  北侧书桌 + 阳台北栏杆
    "#..........#oor",   // y = 3
    "O.....Y....Goor",   // y = 4  过道 + 扫把 + 玻璃门
    "O..........Goor",   // y = 5
    "#..........#oor",   // y = 6
    "#.de.de.de.#rrr",   // y = 7  南侧书桌 + 阳台南栏杆
    "#.Bb.Bb.Bb.###",   // y = 8  南侧床
    "###############",   // y = 9
};

bool isSolidTile(char c) {
    return c == '#' || c == 'B' || c == 'b' ||
        c == 'D' || c == 'E' || c == 'd' || c == 'e' ||
        c == 'O' || c == 'r';
}

// ============================================================
//  地板
// ============================================================
static void drawFloor(sf::RenderTarget& rt, int tx, int ty) {
    const float x = (float)(tx * TILE);
    const float y = (float)(ty * TILE);
    baseTile(rt, tx, ty, sf::Color(180, 142, 96), 10);

    // 横向木板接缝
    for (int i = 1; i <= 3; ++i) {
        sf::RectangleShape line(sf::Vector2f((float)TILE, 2.f));
        line.setPosition(x, y + TILE * (i * 0.25f));
        line.setFillColor(sf::Color(148, 112, 70, 110));
        rt.draw(line);
    }
    // 木纹斑点
    for (int i = 0; i < 4; ++i) {
        const float hx = hash2(tx, ty, 310 + i);
        const float hy = hash2(tx, ty, 410 + i);
        sf::RectangleShape g(sf::Vector2f(8.f, 2.f));
        g.setPosition(x + hx * (TILE - 8.f), y + hy * (TILE - 2.f));
        g.setFillColor(sf::Color(148, 112, 70, 120));
        rt.draw(g);
    }
}

// ============================================================
//  墙壁
// ============================================================
static void drawWall(sf::RenderTarget& rt, int tx, int ty) {
    const float x = (float)(tx * TILE);
    const float y = (float)(ty * TILE);
    baseTile(rt, tx, ty, sf::Color(230, 214, 178), 8);

    // 踢脚线
    sf::RectangleShape base(sf::Vector2f((float)TILE, TILE * 0.22f));
    base.setPosition(x, y + TILE * 0.78f);
    base.setFillColor(sf::Color(158, 134, 96));
    rt.draw(base);

    // 踢脚线阴影
    sf::RectangleShape s(sf::Vector2f((float)TILE, 3.f));
    s.setPosition(x, y + TILE * 0.78f);
    s.setFillColor(sf::Color(128, 106, 74));
    rt.draw(s);
}

// ============================================================
//  床（左半 —— 床头 + 枕头）
// ============================================================
static void drawBedHead(sf::RenderTarget& rt, int tx, int ty) {
    const float x = (float)(tx * TILE);
    const float y = (float)(ty * TILE);

    // 木质床架
    sf::RectangleShape frame(sf::Vector2f((float)TILE, (float)TILE));
    frame.setPosition(x, y);
    frame.setFillColor(sf::Color(126, 86, 52));
    rt.draw(frame);

    // 左侧床头板
    sf::RectangleShape hb(sf::Vector2f(10.f, (float)TILE));
    hb.setPosition(x, y);
    hb.setFillColor(sf::Color(100, 66, 38));
    rt.draw(hb);

    // 床垫
    sf::RectangleShape mat(sf::Vector2f(TILE - 14.f, TILE - 20.f));
    mat.setPosition(x + 10.f, y + 10.f);
    mat.setFillColor(sf::Color(242, 236, 222));
    rt.draw(mat);

    // 枕头
    sf::RectangleShape pil(sf::Vector2f(TILE * 0.55f, TILE * 0.40f));
    pil.setPosition(x + 14.f, y + TILE * 0.30f);
    pil.setFillColor(sf::Color(253, 251, 246));
    rt.draw(pil);

    // 枕头褶皱
    sf::RectangleShape cr(sf::Vector2f(TILE * 0.55f, 2.f));
    cr.setPosition(x + 14.f, y + TILE * 0.50f);
    cr.setFillColor(sf::Color(216, 208, 192));
    rt.draw(cr);
}

// ============================================================
//  床（右半 —— 床尾 + 毯子）
// ============================================================
static void drawBedFoot(sf::RenderTarget& rt, int tx, int ty) {
    const float x = (float)(tx * TILE);
    const float y = (float)(ty * TILE);

    // 木质床架
    sf::RectangleShape frame(sf::Vector2f((float)TILE, (float)TILE));
    frame.setPosition(x, y);
    frame.setFillColor(sf::Color(126, 86, 52));
    rt.draw(frame);

    // 床垫
    sf::RectangleShape mat(sf::Vector2f(TILE - 4.f, TILE - 20.f));
    mat.setPosition(x, y + 10.f);
    mat.setFillColor(sf::Color(242, 236, 222));
    rt.draw(mat);

    // 毯子
    static const sf::Color blankets[3] = {
        sf::Color(178, 76, 62),    // 红
        sf::Color(76, 118, 88),    // 绿
        sf::Color(80, 106, 158),   // 蓝
    };
    const int pick = (int)(hash2(tx, ty, 999) * 3) % 3;
    sf::RectangleShape bl(sf::Vector2f(TILE - 4.f, TILE - 20.f));
    bl.setPosition(x, y + 10.f);
    bl.setFillColor(blankets[pick]);
    rt.draw(bl);

    // 床单折边（毯子左侧）
    sf::RectangleShape fold(sf::Vector2f(6.f, TILE - 20.f));
    fold.setPosition(x, y + 10.f);
    fold.setFillColor(sf::Color(246, 242, 234));
    rt.draw(fold);
}

// ============================================================
//  书桌
//  north = true 时书桌在床的南侧（靠过道的一边朝下）
//  left  = true 表示左半边（放笔记本电脑），false 右半边（放书）
// ============================================================
static void drawDesk(sf::RenderTarget& rt, int tx, int ty, bool north, bool left) {
    const float x = (float)(tx * TILE);
    const float y = (float)(ty * TILE);

    // 桌面
    sf::RectangleShape desk(sf::Vector2f((float)TILE, (float)TILE));
    desk.setPosition(x, y);
    desk.setFillColor(sf::Color(164, 120, 74));
    rt.draw(desk);

    // 木纹
    for (int i = 1; i <= 2; ++i) {
        sf::RectangleShape ln(sf::Vector2f((float)TILE, 2.f));
        ln.setPosition(x, y + TILE * (i / 3.0f));
        ln.setFillColor(sf::Color(140, 100, 60, 130));
        rt.draw(ln);
    }

    // 靠过道一侧的前缘
    sf::RectangleShape edge(sf::Vector2f((float)TILE, 8.f));
    edge.setPosition(x, north ? (y + TILE - 8.f) : y);
    edge.setFillColor(sf::Color(190, 144, 90));
    rt.draw(edge);

    // 桌上的物品
    if (left) {
        // 笔记本电脑
        sf::RectangleShape lap(sf::Vector2f(TILE * 0.60f, TILE * 0.42f));
        lap.setPosition(x + TILE * 0.20f, y + TILE * 0.28f);
        lap.setFillColor(sf::Color(58, 60, 66));
        rt.draw(lap);

        sf::RectangleShape scr(sf::Vector2f(TILE * 0.50f, TILE * 0.30f));
        scr.setPosition(x + TILE * 0.25f, y + TILE * 0.32f);
        scr.setFillColor(sf::Color(126, 184, 218));
        rt.draw(scr);

        // 屏幕反光
        sf::RectangleShape gl(sf::Vector2f(TILE * 0.12f, TILE * 0.26f));
        gl.setPosition(x + TILE * 0.28f, y + TILE * 0.34f);
        gl.setFillColor(sf::Color(210, 238, 252, 110));
        rt.draw(gl);
    }
    else {
        // 叠放的书本
        sf::RectangleShape b1(sf::Vector2f(TILE * 0.36f, TILE * 0.12f));
        b1.setPosition(x + TILE * 0.30f, y + TILE * 0.34f);
        b1.setFillColor(sf::Color(182, 62, 54));
        rt.draw(b1);

        sf::RectangleShape b2(sf::Vector2f(TILE * 0.32f, TILE * 0.10f));
        b2.setPosition(x + TILE * 0.32f, y + TILE * 0.46f);
        b2.setFillColor(sf::Color(72, 112, 152));
        rt.draw(b2);

        sf::RectangleShape b3(sf::Vector2f(TILE * 0.30f, TILE * 0.10f));
        b3.setPosition(x + TILE * 0.33f, y + TILE * 0.56f);
        b3.setFillColor(sf::Color(120, 150, 74));
        rt.draw(b3);
    }
}

// ============================================================
//  宿舍门（西墙）
// ============================================================
static void drawDoor(sf::RenderTarget& rt, int tx, int ty) {
    const float x = (float)(tx * TILE);
    const float y = (float)(ty * TILE);

    sf::RectangleShape wall(sf::Vector2f((float)TILE, (float)TILE));
    wall.setPosition(x, y);
    wall.setFillColor(sf::Color(226, 208, 172));
    rt.draw(wall);

    sf::RectangleShape panel(sf::Vector2f(TILE * 0.82f, TILE * 0.94f));
    panel.setPosition(x + TILE * 0.05f, y + TILE * 0.03f);
    panel.setFillColor(sf::Color(130, 90, 54));
    rt.draw(panel);

    sf::RectangleShape inner(sf::Vector2f(TILE * 0.62f, TILE * 0.72f));
    inner.setPosition(x + TILE * 0.15f, y + TILE * 0.14f);
    inner.setFillColor(sf::Color(114, 78, 46));
    rt.draw(inner);

    sf::CircleShape knob(4.f);
    knob.setPosition(x + TILE * 0.72f, y + TILE * 0.50f - 4.f);
    knob.setFillColor(sf::Color(224, 202, 150));
    rt.draw(knob);
}

// ============================================================
//  阳台玻璃门（东墙）
// ============================================================
static void drawGlassDoor(sf::RenderTarget& rt, int tx, int ty) {
    const float x = (float)(tx * TILE);
    const float y = (float)(ty * TILE);

    sf::RectangleShape frame(sf::Vector2f((float)TILE, (float)TILE));
    frame.setPosition(x, y);
    frame.setFillColor(sf::Color(156, 116, 74));
    rt.draw(frame);

    for (int i = 0; i < 2; ++i) {
        sf::RectangleShape g(sf::Vector2f(TILE * 0.38f, TILE * 0.78f));
        g.setPosition(x + TILE * (0.08f + i * 0.48f), y + TILE * 0.11f);
        g.setFillColor(sf::Color(172, 214, 232, 200));
        rt.draw(g);

        sf::RectangleShape hl(sf::Vector2f(TILE * 0.08f, TILE * 0.78f));
        hl.setPosition(x + TILE * (0.10f + i * 0.48f), y + TILE * 0.11f);
        hl.setFillColor(sf::Color(230, 246, 255, 120));
        rt.draw(hl);
    }
}

// ============================================================
//  阳台地面
// ============================================================
static void drawBalcony(sf::RenderTarget& rt, int tx, int ty) {
    const float x = (float)(tx * TILE);
    const float y = (float)(ty * TILE);
    baseTile(rt, tx, ty, sf::Color(176, 176, 180), 8);

    // 地砖缝
    sf::RectangleShape gv(sf::Vector2f(2.f, (float)TILE));
    gv.setPosition(x + TILE * 0.5f, y);
    gv.setFillColor(sf::Color(146, 146, 152, 120));
    rt.draw(gv);

    sf::RectangleShape gh(sf::Vector2f((float)TILE, 2.f));
    gh.setPosition(x, y + TILE * 0.5f);
    gh.setFillColor(sf::Color(146, 146, 152, 120));
    rt.draw(gh);
}

// ============================================================
//  水平栏杆
// ============================================================
static void drawRailingH(sf::RenderTarget& rt, int tx, int ty, bool atBottom) {
    const float x = (float)(tx * TILE);
    const float y = (float)(ty * TILE);
    const float barY = atBottom ? (y + TILE - 10.f) : y;

    sf::RectangleShape bar(sf::Vector2f((float)TILE, 10.f));
    bar.setPosition(x, barY);
    bar.setFillColor(sf::Color(124, 128, 136));
    rt.draw(bar);

    sf::RectangleShape hl(sf::Vector2f((float)TILE, 3.f));
    hl.setPosition(x, barY);
    hl.setFillColor(sf::Color(156, 160, 168));
    rt.draw(hl);

    for (int i = 0; i < 3; ++i) {
        sf::RectangleShape post(sf::Vector2f(6.f, 12.f));
        post.setPosition(x + TILE * (0.15f + i * 0.35f),
            atBottom ? (barY - 8.f) : (barY + 10.f));
        post.setFillColor(sf::Color(110, 114, 122));
        rt.draw(post);
    }
}

// ============================================================
//  垂直栏杆
// ============================================================
static void drawRailingV(sf::RenderTarget& rt, int tx, int ty) {
    const float x = (float)(tx * TILE);
    const float y = (float)(ty * TILE);

    sf::RectangleShape bar(sf::Vector2f(10.f, (float)TILE));
    bar.setPosition(x + TILE - 10.f, y);
    bar.setFillColor(sf::Color(124, 128, 136));
    rt.draw(bar);

    sf::RectangleShape hl(sf::Vector2f(3.f, (float)TILE));
    hl.setPosition(x + TILE - 10.f, y);
    hl.setFillColor(sf::Color(156, 160, 168));
    rt.draw(hl);

    for (int i = 0; i < 3; ++i) {
        sf::RectangleShape post(sf::Vector2f(12.f, 6.f));
        post.setPosition(x + TILE - 18.f, y + TILE * (0.15f + i * 0.35f));
        post.setFillColor(sf::Color(110, 114, 122));
        rt.draw(post);
    }
}

// ============================================================
//  扫把
// ============================================================
static void drawBroom(sf::RenderTarget& rt, int tx, int ty) {
    const float x = (float)(tx * TILE);
    const float y = (float)(ty * TILE);

    // 手柄
    sf::RectangleShape handle(sf::Vector2f(6.f, TILE * 0.52f));
    handle.setPosition(x + TILE * 0.47f, y + TILE * 0.10f);
    handle.setFillColor(sf::Color(178, 136, 80));
    rt.draw(handle);

    // 刷毛
    sf::CircleShape brist(TILE * 0.17f, 6);
    brist.setPosition(x + TILE * 0.33f, y + TILE * 0.58f);
    brist.setFillColor(sf::Color(154, 128, 68));
    rt.draw(brist);

    // 绑扎带
    sf::RectangleShape band(sf::Vector2f(TILE * 0.22f, 6.f));
    band.setPosition(x + TILE * 0.39f, y + TILE * 0.56f);
    band.setFillColor(sf::Color(120, 96, 52));
    rt.draw(band);

    // 刷毛尖
    sf::RectangleShape tip(sf::Vector2f(TILE * 0.24f, TILE * 0.10f));
    tip.setPosition(x + TILE * 0.38f, y + TILE * 0.73f);
    tip.setFillColor(sf::Color(130, 106, 56));
    rt.draw(tip);
}

// ============================================================
//  主入口：绘制一格
// ============================================================
void drawTile(sf::RenderTarget& rt, char c, int tx, int ty) {
    switch (c) {
    case '#': drawWall(rt, tx, ty); break;
    case '.': drawFloor(rt, tx, ty); break;
    case 'O': drawDoor(rt, tx, ty); break;
    case 'G': drawGlassDoor(rt, tx, ty); break;
    case 'o': drawBalcony(rt, tx, ty); break;

    case 'r': {
        if (tx == 14 && ty >= 3 && ty <= 6)
            drawRailingV(rt, tx, ty);
        else
            drawRailingH(rt, tx, ty, ty <= 4);
        break;
    }

    case 'Y':
        drawFloor(rt, tx, ty);
        drawBroom(rt, tx, ty);
        break;

    case 'B': drawBedHead(rt, tx, ty); break;
    case 'b': drawBedFoot(rt, tx, ty); break;

    case 'D': drawDesk(rt, tx, ty, true, true);  break;   // 北侧书桌，左
    case 'E': drawDesk(rt, tx, ty, true, false); break;   // 北侧书桌，右
    case 'd': drawDesk(rt, tx, ty, false, true);  break;   // 南侧书桌，左
    case 'e': drawDesk(rt, tx, ty, false, false); break;   // 南侧书桌，右

    default: drawFloor(rt, tx, ty); break;
    }
}