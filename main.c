#include "util.h"
#include "raylib.h"
#include "raymath.h"
#include "map.h"
#include "world.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>

#define PAL1 (Color) {  11,  16,  22, 255 }
#define PAL2 (Color) {  20,  31,  37, 255 }
#define PAL3 (Color) {  31,  45,  54, 255 }
#define PAL4 (Color) {  33,  37,  40, 255 }

#define PAL7 (Color) {  86,  41,  35, 255 }
#define PAL9 (Color) { 102,  53,  46, 255 }

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

#define HRO_COLBOUNCE    -0.4

#define CAM_BACK         5
#define CAM_Z            2.5
#define CAM_LOOKZ        -0.1

// ASSETS
typedef struct {
    int w, h;
    Color* p;
} Sprite;

#define X(name) Sprite sprite_##name;
#include "assets.h"
#undef X 

void
loadass() {
#define X(name) \
    Image img_##name = LoadImage("./" #name ".png"); \
    sprite_##name.w = img_##name.width ; \
    sprite_##name.h = img_##name.height; \
    sprite_##name.p = LoadImageColors(img_##name);
#include "assets.h"
#undef X 
}

#define GAMEW 160
#define GAMEH 90
#define ASPECT (float)GAMEW / (float)GAMEH
#define TREESCALE 8

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

void 
printv3(Vector3 vec) {
    printf("(%f, %f, %f)\n", vec.x, vec.y, vec.z);
}

void
drawbillboard(Vector3 billpos, Sprite sprite) {
    Quaternion rotate = QuaternionFromAxisAngle((Vector3) {0, 1, 0}, PI/2);
    rotate = QuaternionMultiply(QuaternionFromAxisAngle((Vector3) {1, 0, 0}, -Vector2Angle((Vector2){1, 0}, (Vector2) {billpos.x - cam.pos.x, billpos.y - cam.pos.y})), rotate);
    Vector3 localcampos = Vector3RotateByQuaternion(cam.pos, rotate);
    localcampos = Vector3Add(localcampos, Vector3RotateByQuaternion(Vector3Scale(billpos, -1), rotate));
    Vector3 localcamfrwd = Vector3RotateByQuaternion(cam.frwd, rotate);
    Vector3 localcamside = Vector3RotateByQuaternion(cam.side, rotate);
    Vector3 localcamup = Vector3RotateByQuaternion(cam.up, rotate);

    for (int x = 0; x < GAMEW; x++) {
        for (int y = 0; y < GAMEH; y++) {
            Vector2 uv = {(float)x / GAMEW * 2 - 1, (float)y / GAMEH * 2 - 1};
            uv.x *= ASPECT;
            uv.y *= -1;

            Vector3 ray = localcamfrwd;

            ray = Vector3Add(ray, Vector3Scale(localcamside, uv.x * cam.foc_len));
            ray = Vector3Add(ray, Vector3Scale(localcamup  , uv.y * cam.foc_len));
            if(cam.roll != 0)
                ray = Vector3RotateByAxisAngle(ray, localcamfrwd, cam.roll);
            
            if (ray.z >= -0.001f) {
                continue;
            }

            ray = Vector3Scale(ray, -localcampos.z / ray.z);
            ray = Vector3Add(ray, localcampos);

            int imgx = (int)(ray.y * TREESCALE) + sprite.w / 2;
            int imgy = sprite.h - (int)(ray.x * TREESCALE);

            if(imgx < 0 || imgx >= sprite.w) continue;

            if(imgy < 0 || imgy >= sprite.h) continue;

            Color imgcol = sprite.p[imgy * sprite.w + imgx];

            if(imgcol.a == 0) continue;
            if(1000/Vector3LengthSqr(ray) < depth[y][x]) continue;

            depth[y][x] = 1000/Vector3LengthSqr(ray);
            billpix[y][x] = imgcol;
        }
    }
}

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

            int wx = (int)ray.x + (int)cam.pos.x;
            int wy = (int)ray.y + (int)cam.pos.y;

            Color roadcol;
            if (isroad(wx, wy)) {
                int r = ((wx & 0b111) ^ (wy & 0b111)) * 0x10193;
                if (r & 0x800) roadcol = PAL2;
                else           roadcol = PAL1;
            } else {
                if ((wx + wy) % 6 > 3) roadcol = PAL7;
                else                   roadcol = PAL9;
            }

            grndpix[y][x] = roadcol;
        }
    }
}

void
drawhro() {
    float ang = Vector2Angle(hro.mom, hro.to);
    Sprite sprite = sprite_carfrwd;

    if (fabsf(ang) < 3) {
        if (hro.touched > HRO_TIMETOUCH2) {
            if      (ang < -1.544) sprite = sprite_cardriftr2;
            else if (ang >  1.544) sprite = sprite_cardriftl2;
        } else if (hro.touched) {
            if      (ang < -1.544) sprite = sprite_cardriftr1;
            else if (ang >  1.544) sprite = sprite_cardriftl1;
        } else {
            if      (ang < -1.544) sprite = sprite_carturnr;
            else if (ang >  1.544) sprite = sprite_carturnl;
        }
    }

    int sx = GAMEW/2 - sprite.w / 2;
    int sy = GAMEH - sprite.h;
    for (int x = 0; x < sprite.w; x++)
        for (int y = 0; y < sprite.h; y++) {
            if (sprite.p[y*sprite.w+x].a == 0) continue;
            grndpix[sy+y][sx+x] = sprite.p[y*sprite.w+x];
        }
}

void
docam() {
    cam.pos.x = hro.pos.x - hro.mom.x * CAM_BACK;
    cam.pos.y = hro.pos.y - hro.mom.y * CAM_BACK;
    cam.pos.z = CAM_Z;
    cam.frwd.x = hro.mom.x;
    cam.frwd.y = hro.mom.y;
    cam.frwd.z = CAM_LOOKZ;

    cam.frwd = Vector3Normalize(cam.frwd);
    cam.side = Vector3CrossProduct((Vector3){0, 0, 1}, cam.frwd);
    cam.up   = Vector3CrossProduct(cam.frwd, cam.side);
    
    cam.foc_len = 1.0 / tan(cam.fov / 2.0);
}
void
dohro() {
    if (IsKeyDown(KEY_R)) hro.sp = 0.04;

    if (hro.mom.x == 0 && hro.mom.y == 0) hro.mom = (Vector2) {1,0};

    bool left = IsKeyDown(KEY_LEFT ) || IsKeyDown(KEY_A);
    bool right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
    if      (left && !right)
        hro.to = Vector2Rotate(hro.mom, -PI/2);
    else if (right && !left)
        hro.to = Vector2Rotate(hro.mom,  PI/2);
    else
        hro.to = (Vector2){0};

    hro.mom = Vector2Scale(hro.mom, MAX(fabsf(hro.sp), 0.3));
    hro.mom = Vector2Add(hro.mom, Vector2Scale(hro.to, HRO_TURNACC));
    hro.mom = Vector2Normalize(hro.mom);

    if (IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_J)) {
        hro.touched += 1;
        hro.prevtouched = hro.touched;
        hro.raised = 0;
    } else {
        hro.raised += 1;
        hro.touched = 0;
    }

    if (hro.touched == 0) {
        hro.sp = (hro.sp + HRO_LINACC);
        if (hro.sp > 0) hro.sp *= HRO_LINDAMP;
    }
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

    Vector2 next = Vector2Add(hro.pos, Vector2Scale(hro.mom, hro.sp));
    if (isroad((int)next.x, (int)next.y)) {
        hro.pos = next;
    } else {
        hro.sp = HRO_COLBOUNCE * SGN(hro.sp);
    }
}

int 
main(void) {
    SetTraceLogLevel(LOG_ERROR); 

    loadass();
    loadmap();

    InitWindow(800, 450, "raylib [core] example - basic window");
    SetWindowState(FLAG_WINDOW_RESIZABLE);

    Image img = GenImageColor(GAMEW, GAMEH, BLANK);
    Texture2D grndtex = LoadTextureFromImage(img);
    Texture2D billtex = LoadTextureFromImage(img);
    Texture2D parttex = LoadTextureFromImage(img);

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        dohro();
        docam();

        //Clear pixel buffers
        memset(billpix, 0, sizeof(billpix));
        memset(partpix, 0, sizeof(partpix));
        memset(grndpix, 0, sizeof(grndpix));
        memset(depth, 0, sizeof(depth));

        //draw trees
        int index = 0;
        do{
            drawbillboard((Vector3) {treeloc[index].x, treeloc[index].y, 0}, sprite_eviltree);
            index++;
        }
        while(!Vector2Equals(treeloc[index], Vector2Zero()));
        
        drawground();
        drawhro();
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
           // DrawTexturePro(parttex, gamerect, winrect, Vector2Zero(), 0, tint);
            DrawTexturePro(grndtex, gamerect, winrect, Vector2Zero(), 0, tint);
            DrawTexturePro(billtex, gamerect, winrect, Vector2Zero(), 0, tint);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
