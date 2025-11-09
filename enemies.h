#include "raylib.h"

enum EnemyType {
  SPEAR,
  THWOMP,
  WORM,
  ZIGGYWORM
};

typedef struct Enemy Enemy;
struct Enemy {
    enum EnemyType type;
    Vector2 pos;
	float road;
	Vector2 trigger;
	float delay;
	int timer;
	bool triggered;
    bool gone;
};

Enemy
enemies[] = {
	{SPEAR, {1100, 360}, 4, {1072, 360}, 110, -1, false, false},
	{SPEAR, {1180, 370}, 4, {1150, 370}, 110, -1, false, false},
	{SPEAR, {1280, 376}, 4, {1250, 376}, 110, -1, false, false},
	{SPEAR, {1280, 355}, 4, {1250, 355}, 110, -1, false, false},
	{SPEAR, {1394, 284}, 4, {1330, 350}, 240, -1, false, false},
	{SPEAR, {1430, 310}, 4, {1380, 290}, 240, -1, false, false},
	{SPEAR, {1450, 310}, 4, {1380, 290}, 240, -1, false, false},
	{THWOMP, {64, 0}, 0, {50, 50}, 10, -1, false, false}
};
int enemycount = 7;
