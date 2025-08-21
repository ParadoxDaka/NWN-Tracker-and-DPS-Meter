#pragma once

struct LevelInfo
{
    int currentLevel;
    int xpCurrentLevelStart;
    int xpToNextLevel;
};

const int NWN_LEVEL_MAX = 40;
extern const int XP_TABLE[NWN_LEVEL_MAX + 1];

LevelInfo GetLevelFromXP(int currentXP);

