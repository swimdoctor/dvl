#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "world.h"

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
GAMEW = 160,
GAMEH = 90;

#define GAMEW 320
#define GAMEH 180
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
drawbillboard(Vector3 billpos, Color* image) {
    Quaternion rotate = QuaternionFromAxisAngle((Vector3) {0, 1, 0}, PI/2);
    rotate = QuaternionMultiply(QuaternionFromAxisAngle((Vector3) {1, 0, 0}, -Vector2Angle((Vector2){1, 0}, (Vector2) {billpos.x - cam.pos.x, billpos.y - cam.pos.y})), rotate);
    Vector3 localcampos = Vector3RotateByQuaternion(cam.pos, rotate);
    localcampos = Vector3Add(localcampos, Vector3RotateByQuaternion(Vector3Scale(billpos, -1), rotate));
    Vector3 localcamfrwd = Vector3RotateByQuaternion(cam.frwd, rotate);
    Vector3 localcamside = Vector3RotateByQuaternion(cam.side, rotate);
    Vector3 localcamup = Vector3RotateByQuaternion(cam.up, rotate);

    // printf("Transformed Vectors:\n");
    // printv3(localcampos);
    // printv3(cam.frwd);
    // printv3(localcamfrwd);
    // printv3(localcamside);
    // printv3(localcamup);
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    //ToggleFullscreen();

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
            
            if (ray.z >= -0.0f) {
                continue;
            }

            ray = Vector3Scale(ray, -localcampos.z / ray.z);
            ray = Vector3Add(ray, localcampos);

            
            int imgx = (int)(ray.y * TREESCALE) + asset_eviltree.width / 2;
            int imgy = asset_eviltree.height - (int)(ray.x * TREESCALE);

            if(imgx < 0 || imgx >= asset_eviltree.width) continue;

            if(imgy < 0 || imgy >= asset_eviltree.height) continue;

            Color imgCol = image[imgy * asset_eviltree.width + imgx];

            if(imgCol.a == 0) continue;
            if(1000/Vector3LengthSqr(ray) < depth[y][x]) continue;

            depth[y][x] = 1000/Vector3LengthSqr(ray);
            billpix[y][x] = imgCol;
        }
    }
}


void
drawground() {
    for (int x = 0; x < GAMEW; x++) {
        for (int y = 0; y < GAMEH; y++) {
            if (depth[y][x] > GROUND_DEPTH) continue;

            Vector2 uv = {(float)x / GAMEW * 2 - 1, (float)y / GAMEH * 2 - 1};
            uv.x *= ASPECT;
            uv.y *= -1;

            Vector3 ray = cam.frwd;
            ray = Vector3Add(ray, Vector3Scale(cam.side, uv.x * cam.foc_len));
            ray = Vector3Add(ray, Vector3Scale(cam.up  , uv.y * cam.foc_len));
            if(cam.roll != 0)
                ray = Vector3RotateByAxisAngle(ray, cam.frwd, cam.roll);
            
            if (ray.z >= -0.0f) {
                grndpix[y][x] = sky;
                continue;
            }

            ray = Vector3Scale(ray, -cam.pos.z / ray.z);
            Vector3 groundIntersect = Vector3Add(ray, cam.pos);

            int grid = abs((int)groundIntersect.x) + abs((int)groundIntersect.y);
            if(groundIntersect.x * groundIntersect.y < 0) grid++;
            grid %= 2;

            grndpix[y][x] = (Color) { grid * 255, grid * 255, grid * 255, 255 };
        }
    }
}

void
docam() {
    cam.pos.x = hro.pos.x / 100;
    cam.pos.y = hro.pos.y / 100;
    cam.pos.z = 1 / (hro.touched*0.1 + 0.4) + 0.5;
    cam.frwd.x = hro.mom.x;
    cam.frwd.y = hro.mom.y;

    cam.frwd = Vector3Normalize(cam.frwd);
    if(Vector3Equals(cam.frwd, Vector3Zero())) cam.frwd = (Vector3) {1, 0, 0};
    cam.side = Vector3CrossProduct((Vector3){0, 0, 1}, cam.frwd);
    cam.up   = Vector3CrossProduct(cam.frwd, cam.side);
    
    cam.foc_len = 1.0 / tan(cam.fov / 2.0);

    //printf("pos = (%f, %f)\n", cam.pos.x, cam.pos.y);
}

#define HRO_SZ           16
#define HRO_TURNACC      0.3
#define HRO_ACC          0.15
#define HRO_LINDAMP      0.98
#define HRO_TIMETOUCH    3
#define HRO_TIMETOUCH1   20
#define HRO_TIMETOUCH2   40
#define HRO_TIMETOUCH3   80
#define HRO_PRETOUCHDAMP 0.85
#define HRO_TOUCHSP      20

#define HRO_TIMERAISE    3
#define HRO_RAISEBOOST1  1.4
#define HRO_RAISEBOOST2  2
#define HRO_RAISEBOOST3  2.4

void
dohro() {
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
}

int 
main(void) {
    loadass();

    InitWindow(800, 450, "raylib [core] example - basic window");

    Color* treecol = LoadImageColors(asset_eviltree);

    Image img = GenImageColor(GAMEW, GAMEH, BLANK);

    
    Texture2D grndtex = LoadTextureFromImage(img);
    Texture2D billtex = LoadTextureFromImage(img);
    Texture2D parttex = LoadTextureFromImage(img);

    float billboardPos = 0;

    //SetTargetFPS(60);
    while (!WindowShouldClose()) {
        dohro();
        docam();
        
        if(IsKeyDown(KEY_F)) billboardPos -= 0.1f;
        if(IsKeyDown(KEY_H)) billboardPos += 0.1f;

        //Clear pixel buffers

        
        for (int x = 0; x < GAMEW; x++) {
            for (int y = 0; y < GAMEH; y++) {
                billpix[y][x].a = 0;
                partpix[y][x].a = 0;
                grndpix[y][x].a = 0;
                depth[y][x] = 0;
            }
        }  

        //draw trees
        int index = 0;
        do{
            drawbillboard((Vector3) {treeloc[index].x, treeloc[index].y, 0}, treecol);
            index++;
        }
        while(!Vector2Equals(treeloc[index], Vector2Zero()));
        
        drawground();
        UpdateTexture(billtex, billpix);
        UpdateTexture(parttex, partpix);
        UpdateTexture(grndtex, grndpix);

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
            DrawTexturePro(grndtex, gamerect, winrect, Vector2Zero(), 0, tint);
            DrawTexturePro(billtex, gamerect, winrect, Vector2Zero(), 0, tint);
            //DrawTexturePro(parttex, gamerect, winrect, Vector2Zero(), 0, tint);
        EndDrawing();

        printf("%i\n", GetFPS());
    }

    CloseWindow();

    return 0;
}
