#include "raylib.h"
#include "raymath.h"
#include "map.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// ASSETS
#define X(name, path) Image name;
#include "assets.h"
#undef X 

void
loadass() {
#define X(name, path) name = LoadImage("./" path);
#include "assets.h"
#undef X 
}

const int
GAMEW = 320,
GAMEH = 180;

#define GAMEW 320
#define GAMEH 180
#define ASPECT (float)GAMEW / (float)GAMEH

const Color 
sky = {100, 100, 255, 255};

Color
grndpix[GAMEH][GAMEW] = {0},
billpix[GAMEH][GAMEW] = {0},
partpix[GAMEH][GAMEW] = {0};
float
depth[GAMEH][GAMEW];

#define GROUND_DEPTH 0

struct {
    Vector2 pos;
    Vector2 mom;
    Vector2 to;
    Vector2 vel;
    float sp;
    int prevtouched;
    int touched;
    int raised;
} 
hro = {0};

struct {
    Vector3 pos;
    Vector3 frwd;
    Vector3 side;
    Vector3 up;
    float roll;
    float fov;
    float foc_len;
} 
cam = {
    .pos = {0,0,5},
    .frwd = {1,0,0},
    .fov = PI / 2,
};

/* Returns {0} if ray does not intersect ground */
Vector3
raytoground(int x, int y) { 
    Vector2 uv = {(float)x / GAMEW * 2 - 1, (float)y / GAMEH * 2 - 1};
    uv.x *= ASPECT;
    uv.y *= -1;

    Vector3 ray = cam.frwd;
    ray = Vector3Add(ray, Vector3Scale(cam.side, uv.x * cam.foc_len));
    ray = Vector3Add(ray, Vector3Scale(cam.up  , uv.y * cam.foc_len));
    ray = Vector3RotateByAxisAngle(ray, cam.frwd, cam.roll);
    ray = Vector3Normalize(ray);

    if (ray.z >= -0.0f) {
        return Vector3Zero();
    }

    ray = Vector3Scale(ray, -cam.pos.z / ray.z);

    return ray;
}

void
drawground() {
    for (int x = 0; x < GAMEW; x++) {
        for (int y = 0; y < GAMEH; y++) {
            if (depth[y][x] > GROUND_DEPTH) continue;
            Vector3 ray = raytoground(x, y);

            if (ray.z == 0) {
                grndpix[y][x] = sky;
                continue;
            }

            Vector3 intersect = Vector3Add(ray, cam.pos);

            Color roadcol = RED;
            if (isroad((int)intersect.x, (int)intersect.y))
                roadcol = BLUE;

            if ((int)intersect.x % 2 == 0) roadcol.b = 255;
            if ((int)intersect.y % 2 == 0) roadcol.b = 128;

            grndpix[y][x] = roadcol;
        }
    }
}

void
docam() {
    cam.pos.x = hro.pos.x;
    cam.pos.y = hro.pos.y;
    cam.pos.z = 1 / (hro.touched*0.1 + 0.4) + 0.5;
    cam.frwd.x = hro.mom.x;
    cam.frwd.y = hro.mom.y;

    cam.frwd = Vector3Normalize(cam.frwd);
    cam.side = Vector3CrossProduct((Vector3){0, 0, 1}, cam.frwd);
    cam.up   = Vector3CrossProduct(cam.frwd, cam.side);
    
    cam.foc_len = 1.0 / tan(cam.fov / 2.0);
}

#define HRO_SZ           16
#define HRO_TURNACC      0.01
#define HRO_LINACC       0.015
#define HRO_LINDAMP      0.98
#define HRO_TIMETOUCH    3
#define HRO_TIMETOUCH1   20
#define HRO_TIMETOUCH2   40
#define HRO_TIMETOUCH3   80
#define HRO_PRETOUCHDAMP 0.85
#define HRO_TOUCHSP      0.4

#define HRO_TIMERAISE    3
#define HRO_RAISEBOOST1  1.4
#define HRO_RAISEBOOST2  2
#define HRO_RAISEBOOST3  2.4

void
dohro() {
    if (IsKeyDown(KEY_R)) hro.sp = 0.04;

    if (hro.mom.x == 0 && hro.mom.y == 0) hro.mom = (Vector2) {1,0};

    Vector2 frwd = Vector2Normalize(hro.mom);

    bool left = IsKeyDown(KEY_LEFT ) || IsKeyDown(KEY_A);
    bool right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
    if      (left && !right)
        hro.to = Vector2Rotate(frwd, -PI/2);
    else if (right && !left)
        hro.to = Vector2Rotate(frwd,  PI/2);
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
        hro.sp = (hro.sp + HRO_LINACC) * HRO_LINDAMP;
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
}

int 
main(void) {
    loadass();
    loadmap();

    InitWindow(800, 450, "raylib [core] example - basic window");

    Image img = GenImageColor(GAMEW, GAMEH, BLANK);
    Texture2D grndtex = LoadTextureFromImage(img);
    Texture2D billtex = LoadTextureFromImage(img);
    Texture2D parttex = LoadTextureFromImage(img);

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        markframe();
        dohro();
        docam();
        
        drawground();
        UpdateTexture(grndtex, grndpix);
        UpdateTexture(billtex, billpix);
        UpdateTexture(parttex, partpix);

        BeginDrawing();
            ClearBackground(RAYWHITE);

            Color tint = WHITE;
            if (hro.touched > HRO_TIMETOUCH3)
                tint = RED;
            else if (hro.touched > HRO_TIMETOUCH2)
                tint = GREEN;
            else if (hro.touched > HRO_TIMETOUCH1)
                tint = BLUE;
            Rectangle gamerect = (Rectangle){0,0,GAMEW,GAMEH};
            Rectangle winrect  = (Rectangle){0,0,GetScreenWidth(),GetScreenHeight()};
            //DrawTexturePro(parttex, gamerect, winrect, Vector2Zero(), 0, tint);
            //DrawTexturePro(billtex, gamerect, winrect, Vector2Zero(), 0, tint);
            DrawTexturePro(grndtex, gamerect, winrect, Vector2Zero(), 0, tint);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
