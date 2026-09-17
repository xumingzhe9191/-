#include "Player.hpp"
#include "Config.hpp"

#include <cmath>
#include <iostream>

bool Player::load(const std::string& path) {
    if (!tex.loadFromFile(path)) return false;

    tex.setSmooth(false);

    const sf::Vector2u size = tex.getSize();

    const int STD_W = 16;
    const int STD_H = 32;

    if (size.x % STD_W == 0 && size.y % STD_H == 0) {
        frameW = STD_W;
        frameH = STD_H;
    }
    else {
        frameH = (int)size.y / 4;
        if (frameH <= 0) frameH = 32;
        frameW = frameH / 2;
        if (frameW <= 0) frameW = 16;
    }

    framesInRow = (int)size.x / frameW;
    if (framesInRow < 1) framesInRow = 1;

    scale = (TILE * 2.0f) / (float)frameH;

    sprite.setTexture(tex);
    sprite.setOrigin(frameW / 2.f, (float)frameH);
    sprite.setScale(scale, scale);

    loaded = true;

    const int totalRows = (int)size.y / frameH;
    std::cout << "[Player] 贴图 " << size.x << "x" << size.y
        << "，帧 " << frameW << "x" << frameH
        << "，每行 " << framesInRow << " 帧"
        << "，共 " << totalRows << " 行"
        << "，缩放 " << scale << std::endl;
    return true;
}

sf::FloatRect Player::hitbox() const {
    const float w = TILE * 0.62f;
    const float h = TILE * 0.38f;
    return sf::FloatRect(pos.x - w * 0.5f, pos.y - h, w, h);
}

void Player::update(float dt,
    const std::function<bool(const sf::FloatRect&)>& collideFn) {
    sf::Vector2f dir(0.f, 0.f);
    using K = sf::Keyboard;
    if (K::isKeyPressed(K::A) || K::isKeyPressed(K::Left))  dir.x -= 1.f;
    if (K::isKeyPressed(K::D) || K::isKeyPressed(K::Right)) dir.x += 1.f;
    if (K::isKeyPressed(K::W) || K::isKeyPressed(K::Up))    dir.y -= 1.f;
    if (K::isKeyPressed(K::S) || K::isKeyPressed(K::Down))  dir.y += 1.f;

    const bool moving = (dir.x != 0.f || dir.y != 0.f);

    // ---- 每个方向的起始行 & 该方向可用帧数 ----
    // 向下：行 0，4 帧
    // 向上：行 4，4 帧
    // 向右：行 8，3 帧
    // 向左：行 11，2 帧
    int dirIndex = 0;   // 0下 1上 2右 3左
    if (moving) {
        if (dir.x > 0.f)      dirIndex = 2;
        else if (dir.x < 0.f) dirIndex = 3;
        else if (dir.y > 0.f) dirIndex = 0;
        else                  dirIndex = 1;

        static const int DIR_ROW[4] = { 0, 2 , 1, 3 };
        static const int DIR_FRAMES[4] = { 4, 4, 4, 4 };
        dirRow = DIR_ROW[dirIndex];
        dirFrames = DIR_FRAMES[dirIndex];
    }

    if (moving) {
        const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        dir /= len;

        float speed = PLAYER_SPEED;
        if (sprint && sprintDir == dirIndex)
            speed *= SPRINT_MULT;
        const sf::Vector2f d = dir * speed * dt;

        if (d.x != 0.f) {
            const float old = pos.x;
            pos.x += d.x;
            if (collideFn(hitbox())) pos.x = old;
        }
        if (d.y != 0.f) {
            const float old = pos.y;
            pos.y += d.y;
            if (collideFn(hitbox())) pos.y = old;
        }
    }

    // ---- 走路帧序列 ----
    static const int walkSeq[4] = { 0, 1, 2, 1 };

    if (moving) {
        animTimer += dt;
        while (animTimer >= ANIM_INTERVAL) {
            animTimer -= ANIM_INTERVAL;
            animStep = (animStep + 1) % 4;
        }

        // 关键：用 % dirFrames 保证不越界
        // 向下/上(4帧)：0,1,2,1
        // 向右(3帧)  ：0,1,2,1 % 3 = 0,1,2,1
        // 向左(2帧)  ：0,1,2,1 % 2 = 0,1,0,1
        frame = walkSeq[animStep] % dirFrames;
    }
    else {
        animTimer = 0.f;
        animStep = 0;
        frame = 0;   // 站立帧
    }
}

void Player::draw(sf::RenderTarget& rt) {
    if (!loaded) return;
    sprite.setTextureRect(
        sf::IntRect(frame * frameW, dirRow * frameH, frameW, frameH));
    sprite.setPosition(std::round(pos.x), std::round(pos.y));
    rt.draw(sprite);
}

sf::FloatRect Player::debugBox() const {
    return hitbox();
}