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
	{SPEAR, {1430, 310}, 4, {1380, 290}, 200, -1, false, false},
	{SPEAR, {1450, 310}, 4, {1380, 290}, 200, -1, false, false},
	{THWOMP, {1500, 425}, 4, {1430, 425}, 200, -1, false, false},
	{THWOMP, {1600, 425}, 4, {1500, 425}, 200, -1, false, false},
	{THWOMP, {1650, 445}, 4, {1500, 425}, 250, -1, false, false},
	{THWOMP, {1700, 425}, 4, {1500, 425}, 320, -1, false, false},
	{THWOMP, {1750, 445}, 4, {1500, 425}, 380, -1, false, false},

	{THWOMP, {1870, 425}, 4, {1800, 425}, 200, -1, false, false},
	{THWOMP, {1870, 435}, 4, {1800, 425}, 200, -1, false, false},
	{THWOMP, {1920, 435}, 4, {1800, 425}, 250, -1, false, false},
	{THWOMP, {1920, 445}, 4, {1800, 425}, 250, -1, false, false},
	{THWOMP, {1970, 435}, 4, {1800, 425}, 300, -1, false, false},
	{THWOMP, {1970, 445}, 4, {1800, 425}, 300, -1, false, false},
	//{THWOMP, {64, 0}, 0, {50, 50}, 10, -1, false, false}
};
int enemycount = 16;
