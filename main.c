#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

const int
GAMEW = 320,
GAMEH = 180;

struct {
    Vector2 pos;
    Vector2 mom;
    Vector2 to;
    Vector2 vel;
    float sp;
    int prevtouched;
    int touched;
    int raised;
} hro = {0};

#define HRO_SZ           16
#define HRO_TURNACC      0.3
#define HRO_ACC          0.2
#define HRO_LINDAMP      0.98
#define HRO_TIMETOUCH    3
#define HRO_TIMETOUCH1   20
#define HRO_TIMETOUCH2   40
#define HRO_TIMETOUCH3   80
#define HRO_PRETOUCHDAMP 0.85
#define HRO_TOUCHSP      6

#define HRO_TIMERAISE    3
#define HRO_RAISEBOOST1  2
#define HRO_RAISEBOOST2  3
#define HRO_RAISEBOOST3  4

void
do_hro() {
    if (hro.mom.x == 0 && hro.mom.y == 0) hro.mom = (Vector2) {1,0};

    Vector2 frwd = Vector2Normalize(hro.mom);

    bool left = IsKeyDown(KEY_LEFT ) || IsKeyDown(KEY_A);
    bool right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
    if      (left && !right)
        hro.to = Vector2Rotate(frwd,  PI/2);
    else if (right && !left)
        hro.to = Vector2Rotate(frwd, -PI/2);
    else
        hro.to = (Vector2){0};

    hro.vel = Vector2Add(Vector2Scale(hro.mom, hro.sp), Vector2Scale(hro.to, HRO_TURNACC));
    hro.mom = Vector2Normalize(hro.vel);

    if (IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_J)) {
        hro.touched += 1;
        hro.prevtouched = hro.touched;
        hro.raised = 0;
    } else {
        hro.raised += 1;
        hro.touched = 0;
    }

    if (hro.touched == 0)
        hro.sp = (hro.sp + HRO_ACC) * HRO_LINDAMP;
    else if (hro.touched < HRO_TIMETOUCH)
        hro.sp = hro.sp * HRO_PRETOUCHDAMP;
    else 
        hro.sp = HRO_TOUCHSP;

    if (hro.raised == HRO_TIMERAISE) {
        if (hro.prevtouched > HRO_TIMETOUCH3)
            hro.sp *= HRO_RAISEBOOST3;
        else if (hro.prevtouched > HRO_TIMETOUCH2)
            hro.sp *= HRO_RAISEBOOST2;
        else if (hro.prevtouched > HRO_TIMETOUCH1)
            hro.sp *= HRO_RAISEBOOST1;
    }

    hro.pos = Vector2Add(hro.pos, hro.vel);

    if (hro.pos.x < 0)                 hro.pos.x += GetScreenWidth();
    if (hro.pos.x > GetScreenWidth ()) hro.pos.x -= GetScreenWidth();
    if (hro.pos.y < 0)                 hro.pos.y += GetScreenHeight();
    if (hro.pos.y > GetScreenHeight()) hro.pos.y -= GetScreenHeight();
}

int 
main(void) {
    InitWindow(800, 450, "raylib [core] example - basic window");

    Image img = GenImageColor(GAMEW, GAMEH, PINK);
    Texture2D tex = LoadTextureFromImage(img);

    Color *pix = malloc(GAMEW * GAMEH * sizeof(Color));
    for (int x = 0; x < GAMEW; x++)
        for (int y = 0; y < GAMEH; y++)
            pix[y * GAMEW + x] = (Color) { x, y, 0, 255 };

    UpdateTexture(tex, pix);

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        do_hro();

        BeginDrawing();
            ClearBackground(RAYWHITE);

            Color tint = WHITE;
            if (hro.touched > HRO_TIMETOUCH3)
                tint = RED;
            else if (hro.touched > HRO_TIMETOUCH2)
                tint = GREEN;
            else if (hro.touched > HRO_TIMETOUCH1)
                tint = BLUE;
            DrawTexturePro(tex, 
                (Rectangle){0,0,320,180}, (Rectangle){0,0,800,450}, 
                (Vector2){0,0}, 0.0,
                tint
                );

            DrawRectangle((int)hro.pos.x, (int)hro.pos.y, HRO_SZ, HRO_SZ, RED);
            Vector2 look = Vector2Add(hro.pos, Vector2Scale(hro.mom, hro.sp*10));
            DrawRectangle((int)look.x, (int)look.y, HRO_SZ, HRO_SZ, BLUE);
            Vector2 lookm = Vector2Add(hro.pos, Vector2Scale(hro.to, 100));
            DrawRectangle((int)lookm.x, (int)lookm.y, HRO_SZ, HRO_SZ, PINK);
            Vector2 lookv = Vector2Add(hro.pos, Vector2Scale(hro.vel, 10));
            DrawRectangle((int)lookv.x, (int)lookv.y, HRO_SZ, HRO_SZ, GREEN);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
