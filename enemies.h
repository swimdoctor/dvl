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
	float timer;
	bool triggered;
};

Enemy
enemies[] = {
	{SPEAR, {30, 40}, 4, {5, 5}, 5, -1, false},
	{THWOMP, {64, 0}, 0, {50, 50}, 10, -1, false}
};
int enemycount = 2;
