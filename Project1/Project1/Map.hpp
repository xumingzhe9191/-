#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

enum SceneId {
    SCENE_DORM = 0,
    SCENE_HALLWAY = 1,
    SCENE_BALCONY = 2,
    SCENE_SCHOOL = 3,
    SCENE_CAMPUS = 4,
    SCENE_ROOM_A = 5,
    SCENE_ROOM_B = 6,
    SCENE_ROOM_C = 7,
    SCENE_ROOM_D = 8,
    SCENE_ROOM_E = 9,
    SCENE_ROOM_F = 10,
};

const std::vector<std::string>& getMap();
int getMapW();
int getMapH();
int getCurrentScene();
const char* getSceneName(int id);

void setScene(int id);

int checkWarp(int tx, int ty);
void getWarpSpawn(int fromScene, int toScene, int& outTx, int& outTy);

bool isSolidTile(char c);
void drawTile(sf::Image& img, char c, int tx, int ty);
void drawFurniture(sf::Image& img);

// broom interaction
bool hasBroomAt(int tx, int ty);
void pickUpBroom();
bool isBroomPickedUp();
