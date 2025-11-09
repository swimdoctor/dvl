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
#include "enemies.h"

bool paused = true;
int frame = 0;

#define PAL1  (Color) {  11,  16,  22, 255 }
#define PAL2  (Color) {  20,  31,  37, 255 }
#define PAL3  (Color) {  31,  45,  54, 255 }
#define PAL4  (Color) {  33,  37,  40, 255 }

#define PAL7  (Color) {  86,  41,  35, 255 }
#define PAL9  (Color) { 102,  53,  46, 255 }

#define PAL9  (Color) { 102,  53,  46, 255 }
#define PAL10 (Color) {  97,  38,  56, 255 }

#define PAL25 (Color) { 131,  71,  61, 255 }
#define PAL28 (Color) { 144, 124, 117, 255 }

#define PAL30 (Color) { 166, 136,  74, 255 }
#define PAL34 (Color) {  78, 116, 153, 255 }

#define PAL34 (Color) {  78, 116, 153, 255 }
#define PAL36 (Color) { 175, 158, 148, 255 }
#define PAL37 (Color) { 135, 158, 163, 255 }
#define PAL38 (Color) { 189, 187, 174, 255 }
#define PAL39 (Color) { 227, 221, 209, 255 }

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
#define HRO_RAISEBOOST2  2.4
#define HRO_RAISEBOOST3  3

#define HRO_COLBOUNCE    -0.4

#define CAM_BACK         7
#define CAM_KICK         7
#define CAM_FOVKICK      0.5
#define CAM_KICKSPEED    0.1
#define CAM_Z            2.5
#define CAM_LOOKZ        -0.1
#define CAM_ROLLA        0.04
#define CAM_ROLLB        0.12
#define CAM_ROLLC        0.2
#define CAM_ROLLACCA     0.2
#define CAM_ROLLACCB     0.6
#define CAM_ROLLSPEEDA   0.2
#define CAM_ROLLSPEEDB   0.4

// SPRITES
typedef struct {
    int w, h;
    Color* p;
} Sprite;

#define X(name) Sprite sprite_##name;
#include "assets.h"
#undef X 

#define M(name) Music audio_##name;
#define S(name) Sound audio_##name;
#include "sounds.h"
#undef M 
#undef S 

void
loadass() {
#define X(name) \
    Image img_##name = LoadImage("./" #name ".png"); \
    sprite_##name.w = img_##name.width ; \
    sprite_##name.h = img_##name.height; \
    sprite_##name.p = LoadImageColors(img_##name);
#   include "assets.h"
#undef X 
#define M(name) audio_##name = LoadMusicStream("./" #name ".wav");
#define S(name) audio_##name = LoadSound("./" #name ".wav");
#   include "sounds.h"
#undef M
#undef S
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
ptcpix[GAMEH][GAMEW] = {0};
float
depth[GAMEH][GAMEW];

#define GROUND_DEPTH 0

int billcount = 0;
int partcount = 0;

int queuecount = 0;
#define TRIGGERRANGE 50

struct {
    Vector2 pos;
    Vector2 mom;
    Vector2 to;
    Vector2 vel;
    float ang;
    float sp;
    int penalty;
    int prevtouched;
    int touched;
    int raised;
} 
hro = {
    .pos = {32, 32},
};

struct {
    Vector3 pos;
    Vector3 frwd;
    Vector3 side;
    Vector3 up;
    float kick;
    float roll, rollvel;
    float fov;
    float foc_len;
} 
cam = {
    .pos = {0,0,5},
    .frwd = {1,0,0},
    .fov = PI / 2,
};

Vector3
worldtoscreen(Vector3 vec) {
    Vector3 ray = Vector3Subtract(vec, cam.pos);
    if(Vector3DotProduct(ray, cam.frwd) <= 0) return (Vector3) {-1, -1, -1};
    Vector3 scaledray = 
        Vector3Scale(Vector3Normalize(ray), 1/cos(Vector3Angle(ray, cam.frwd)));
    
    Vector2 uv = {Vector3DotProduct(scaledray, cam.side) / cam.foc_len, 
                  Vector3DotProduct(scaledray, cam.up  ) / cam.foc_len};
    
    float x = uv.x;
    x /= (ASPECT);
    x += 1;
    x /= 2;
    x *= GAMEW;

    float y = GAMEH * (uv.y * -1 + 1) / 2;
    partcount++;

    return (Vector3) {x, y, Vector3Length(ray)};
}

void 
printv3(Vector3 vec) {
    printf("(%f, %f, %f)\n", vec.x, vec.y, vec.z);
}

void
drawbillboard(Vector3 billpos, Sprite sprite, int scale) {
    Quaternion rotate = QuaternionFromAxisAngle((Vector3) {0, 1, 0}, PI/2);
    rotate = QuaternionMultiply(QuaternionFromAxisAngle((Vector3) {1, 0, 0}, -Vector2Angle((Vector2){1, 0}, (Vector2) {billpos.x - cam.pos.x, billpos.y - cam.pos.y})), rotate);
    Vector3 localcampos = Vector3RotateByQuaternion(cam.pos, rotate);
    localcampos = Vector3Add(localcampos, Vector3RotateByQuaternion(Vector3Scale(billpos, -1), rotate));
    Vector3 localcamfrwd = Vector3RotateByQuaternion(cam.frwd, rotate);
    Vector3 localcamside = Vector3RotateByQuaternion(cam.side, rotate);
    Vector3 localcamup = Vector3RotateByQuaternion(cam.up, rotate);
    
    billcount++;
    
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

            int imgx = (int)(ray.y * scale) + sprite.w / 2;
            int imgy = sprite.h - (int)(ray.x * scale);

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
                Color skycol;
                if      (y <  8) skycol = PAL28;
                else if (y < 20) skycol = PAL36;
                else if (y < 34) skycol = PAL38;
                else if (y < 50) skycol = PAL39;
                grndpix[y][x] = skycol;
                continue;
            }

            if (fabsf(ray.x) > 99999) continue;
            if (fabsf(ray.y) > 99999) continue;

            int wx = (int)ray.x + (int)cam.pos.x;
            int wy = (int)ray.y + (int)cam.pos.y;

            Color roadcol;
            if (isroad(wx, wy)) {
                int r = ((wx & 0b111) ^ (wy & 0b111)) * 0x10193;
                if (r & 0x800) roadcol = PAL2;
                else           roadcol = PAL1;
            } else {
                if (((wx + wy) & 0b100)) roadcol = PAL7;
                else                     roadcol = PAL9;
            }

            for(int i = 0; i < enemycount; i++) {
                if(enemies[i].triggered && Vector2DistanceSqr(enemies[i].pos, (Vector2){ray.x + cam.pos.x, ray.y + cam.pos.y}) < 4) {
                    int spritex = (int)((ray.x + cam.pos.x - enemies[i].pos.x) * 8); 
                    int spritey = (int)((ray.y + cam.pos.y - enemies[i].pos.y) * 8);

                    spritex += 16;
                    spritey += 16;

                    if(spritex >= 0 && spritex < 32 && spritey >= 0 && spritey < 32){
                        if(sprite_target.p[spritey * sprite_target.w + spritex].a == 255){
                            roadcol = ColorLerp(roadcol, sprite_target.p[spritey * sprite_target.w + spritex], 0.5);
                        }
                    }                    
                }
            }

            grndpix[y][x] = roadcol;
        }
    }
}

void
drawptc(Vector3 pos, Color col, int rot) {
    Vector3 spos = worldtoscreen(pos);
    int x = (int)spos.x, y = (int)spos.y;
    if (0 <= x && x < GAMEW && 0 <= y && y < GAMEH)
        grndpix[y][x] = col;
    switch (rot) {
    case 0: x -= 1; break;
    case 1: x += 1; break;
    case 2: y -= 1; break;
    case 3: y += 1; break;
    }
    if (0 <= x && x < GAMEW && 0 <= y && y < GAMEH)
        grndpix[y][x] = col;
}


void
drawptc3(Vector3 pos, Color col, int rot) {
    Vector3 spos = worldtoscreen(pos);
    int x = (int)spos.x, y = (int)spos.y;
    if (0 <= x && x < GAMEW && 0 <= y && y < GAMEH)
        grndpix[y][x] = col;
    switch (rot) {
    case 0: x -= 1; break;
    case 1: x += 1; break;
    case 2: y -= 1; break;
    case 3: y += 1; break;
    }
    if (0 <= x && x < GAMEW && 0 <= y && y < GAMEH)
        ptcpix[y][x] = col;
}


void
drawptc2(Vector3 pos, Color col) {
    Vector3 spos = worldtoscreen(pos);
    int x = (int)spos.x, y = (int)spos.y;
    if (0 <= x && x < GAMEW && 0 <= y && y < GAMEH)
        ptcpix[y][x] = col;
}

typedef struct {
    Vector3 pos;
    Vector3 vel;
    int life;
    unsigned char rot;
    bool back;
} Touch_Ptc;

#define NTOUCHPTC 60
Touch_Ptc touch_ptcs[NTOUCHPTC] = {0};
int tptcidx = 0;

void
drawhro() {
    if (IsKeyDown(KEY_Y)) return;
    if (cam.pos.z > 10) return;

    Sprite *sprite = &sprite_carfrwd;

    if (hro.pos.x < 8) {
        sprite = &sprite_introcar;
    } else if (hro.pos.x < 32) {
        sprite = &sprite_introcar2;
    } else if (fabsf(hro.ang) < 3) {
        if (hro.touched > HRO_TIMETOUCH2) {
            if      (hro.ang < -1.544) sprite = &sprite_cardriftr2;
            else if (hro.ang >  1.544) sprite = &sprite_cardriftl2;
        } else if (hro.touched) {
            if      (hro.ang < -1.544) sprite = &sprite_cardriftr1;
            else if (hro.ang >  1.544) sprite = &sprite_cardriftl1;
        } else {
            if      (hro.ang < -1.544) sprite = &sprite_carturnr;
            else if (hro.ang >  1.544) sprite = &sprite_carturnl;
        }
    }

    Vector3 hro_frnt = worldtoscreen((Vector3){hro.pos.x, hro.pos.y, 0});
    int hfx = (int)hro_frnt.x, hfy = (int)hro_frnt.y;

    for (Touch_Ptc *ptc = touch_ptcs; ptc <= &touch_ptcs[NTOUCHPTC-1]; ptc++) {
        ptc->pos = Vector3Add(ptc->pos, ptc->vel);
        ptc->vel.z -= 0.2;
        if (ptc->life) ptc->life--;
        ptc->rot = (ptc->rot + 1) % 4;
    }

    if (hro.touched) {
        typedef struct { int x, y; } P;
        P ps[4] = {0};
        int pimax = 3;
        bool back = false;
        Vector3 vel = (Vector3) {
            (float)GetRandomValue(-100,100) / 500,
            (float)GetRandomValue(-100,100) / 500,
            0.3,
        };
        float velz = 0.3;
        if (sprite == &sprite_carfrwd) {
            ps[0] = (P){-12,20}; ps[1] = (P){12,20};
            ps[2] = (P){-12,10}; ps[3] = (P){12,10};
        } else if (sprite == &sprite_cardriftr1) {
            ps[0] = (P){-15,10}; ps[1] = (P){-10,22};
            ps[2] = (P){17,26};
            pimax = 2;
        } else if (sprite == &sprite_cardriftl1) {
            ps[0] = (P){15,10}; ps[1] = (P){10,22};
            ps[2] = (P){-17,26};
            pimax = 2;
        } else if (sprite == &sprite_cardriftr2) {
            ps[0] = (P){-8,6}; ps[1] = (P){6,22};
            pimax = 1;
            vel.z = hro.touched > HRO_TIMETOUCH3 ? 0.9 : 0.5;
            vel.x = vel.x + cam.side.x * 0.1 + hro.mom.x * 0.2;
            vel.y = vel.y + cam.side.y * 0.1 + hro.mom.y * 0.2;
        } else if (sprite == &sprite_cardriftl2) {
            ps[0] = (P){ 8,6}; ps[1] = (P){-6,22};
            pimax = 1;
            vel.z = hro.touched > HRO_TIMETOUCH3 ? 0.9 : 0.5;
            vel.x = vel.x + cam.side.x * 0.1 + hro.mom.x * 0.2;
            vel.y = vel.y + cam.side.y * 0.1 + hro.mom.y * 0.2;
        }
        
        if (ps[0].x != 0)
            for (int i = 0; i < 6; i++) {
                int pi = GetRandomValue(0, pimax);

                touch_ptcs[tptcidx] = (Touch_Ptc) {
                    .pos = Vector3Add(cam.pos, raytoground(hfx+ps[pi].x, hfy+ps[pi].y)),
                    .vel = vel,
                    .life = 100,
                    .back = back,
                };
                tptcidx = (tptcidx + 1) % NTOUCHPTC;
            }
    }
    

    Color touchcol = BLANK;
    if      (hro.touched > HRO_TIMETOUCH3) touchcol = PAL30;
    else if (hro.touched > HRO_TIMETOUCH2) touchcol = PAL25;
    else if (hro.touched >              0) touchcol = PAL37;
    if (touchcol.a)
        for (int i = 0; i < NTOUCHPTC; i++)
            if (touch_ptcs[i].life && touch_ptcs[i].back) 
                drawptc(touch_ptcs[i].pos, touchcol, touch_ptcs[i].rot);

    int minx = hfx - sprite->w / 2, maxx = hfx + sprite->w / 2;
    int miny = hfy, maxy = hfy + sprite->h;
    if (sprite == &sprite_introcar) {
        miny -= 20; maxy -= 20;
    } else if (sprite == &sprite_introcar2) {
        miny -= 20; maxy -= 20;
    }
    for (int x = MAX(minx, 0); x < MIN(maxx, GAMEW); x++)
        for (int y = MAX(miny, 0); y < MIN(maxy, GAMEH); y++) {
            Color p = sprite->p[(y - miny) * sprite->w + (x - minx)];
            if (p.a == 0) continue;
            grndpix[y][x] = p;
        }

    if (touchcol.a)
        for (int i = 0; i < NTOUCHPTC; i++)
            if (touch_ptcs[i].life && !touch_ptcs[i].back) 
                drawptc(touch_ptcs[i].pos, touchcol, touch_ptcs[i].rot);
}

void
drawnotice(Sprite sprite) {
    if (frame % 60 > 30) return;
    for (int x = 0; x < GAMEW; x++)
        for (int y = 0; y < GAMEH; y++)
            ptcpix[y][x] = sprite.p[y*GAMEW+x];
}

void
drawnotice2(Sprite sprite) {
    for (int x = 0; x < GAMEW; x++)
        for (int y = 0; y < GAMEH; y++)
            if (sprite.p[y*GAMEW+x].a)
                ptcpix[y][x] = sprite.p[y*GAMEW+x];
}

void
drawnotice3(Sprite sprite, int ox) {
    for (int x = 0; x < GAMEW; x++)
        for (int y = 0; y < GAMEH; y++) {
            int sx = x - ox;
            if (sx < 0 || sx >= GAMEW) continue;
            if (sprite.p[y*GAMEW+sx].a)
                ptcpix[y][x] = sprite.p[y*GAMEW+sx];
        }
}

void
docam() {
    float kicktarg = MAX(MIN(hro.sp, 1), 0.5) * 2 - 1;
    cam.kick = LERP(cam.kick, kicktarg, CAM_KICKSPEED);
    cam.pos.x = hro.pos.x - hro.mom.x * (CAM_BACK + CAM_KICK * cam.kick);
    cam.pos.y = hro.pos.y - hro.mom.y * (CAM_BACK + CAM_KICK * cam.kick);
    cam.pos.z = CAM_Z;
    cam.frwd.x = hro.mom.x;
    cam.frwd.y = hro.mom.y;
    cam.frwd.z = CAM_LOOKZ;

    if (IsKeyDown(KEY_Y)) {
        cam.pos.z = hro.pos.x;
        cam.pos.z = hro.pos.y;
        cam.pos.z = 500;
        cam.fov = 0.1;
        cam.frwd.x = 0.01;
        cam.frwd.y = 0.01;
        cam.frwd.z = -1;
    } else {
        cam.fov = PI/2 - cam.kick * CAM_FOVKICK;
    }

    float rolltarg = 0;
    if (hro.touched > HRO_TIMETOUCH3) {
        if      (hro.ang < -1.544) rolltarg =  CAM_ROLLC;
        else if (hro.ang >  1.544) rolltarg = -CAM_ROLLC;
    } else if (hro.touched > HRO_TIMETOUCH2) {
        if      (hro.ang < -1.544) rolltarg =  CAM_ROLLB;
        else if (hro.ang >  1.544) rolltarg = -CAM_ROLLB;
    } else if (hro.touched) {
        if      (hro.ang < -1.544) rolltarg =  CAM_ROLLA;
        else if (hro.ang >  1.544) rolltarg = -CAM_ROLLA;
    }
    float rollveltarg = (rolltarg - cam.roll) *
           (hro.touched ? CAM_ROLLACCA : CAM_ROLLACCB);
    float lerps = hro.touched ? CAM_ROLLSPEEDA : CAM_ROLLSPEEDB;
    cam.rollvel = LERP(cam.rollvel, rollveltarg, lerps); 
    cam.roll += cam.rollvel;

    if (hro.pos.x < 32) {
        float lerpby = (float)hro.pos.x / 32;
        cam.frwd = Vector3Lerp((Vector3){0.3,1,-1}, cam.frwd, lerpby);
        cam.pos = Vector3Lerp((Vector3){19, 20, 6}, cam.pos, lerpby);
        cam.roll = LERP(0, cam.roll, lerpby);
        cam.fov = LERP(PI/2 - 0.4, 0.1, lerpby);

#define N 200
        if (frame > 622+N+N) {
        } else if (frame < 622+N){
            cam.pos.z += 50;
        } else {
            float diff = (((float)frame - (622+N)) / N);
            printf("%f\n", diff);
            diff = (1-diff);
            diff *= diff;
            diff *= diff;
            cam.pos.z += diff * 100;
        }
    }

    cam.frwd = Vector3Normalize(cam.frwd);
    cam.side = Vector3CrossProduct((Vector3){0, 0, 1}, cam.frwd);
    cam.up   = Vector3CrossProduct(cam.frwd, cam.side);
    
    cam.foc_len = 1.0 / tan(cam.fov / 2.0);
}

float
rangefalloff(float val, float min, float max, float falloff) {
    if (min <= val && val <= max) return 1;
    
    float lf = falloff * (val - min) + 1;
    if (0 < lf && lf < 1) return 3 * lf * lf - 2 * lf * lf * lf;
    float rf = falloff * (max - val) + 1;
    if (0 < rf && rf < 1) return 3 * rf * rf - 2 * rf * rf * rf;

    return 0;
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

    if (hro.penalty) {
        hro.penalty--;
    } else if (hro.touched == 0) {
        hro.sp = (hro.sp + HRO_LINACC);
        if (hro.sp > 0) hro.sp *= HRO_LINDAMP;
    } else if (hro.touched < HRO_TIMETOUCH) {
        hro.sp = hro.sp * HRO_PRETOUCHDAMP;
    } else {
        hro.sp = HRO_TOUCHSP;
    }

    if (hro.raised == HRO_TIMERAISE && hro.prevtouched) {
        if (hro.prevtouched > HRO_TIMETOUCH3)
            hro.sp *= HRO_RAISEBOOST3;
        else if (hro.prevtouched > HRO_TIMETOUCH2)
            hro.sp *= HRO_RAISEBOOST2;
        else if (hro.prevtouched > HRO_TIMETOUCH1)
            hro.sp *= HRO_RAISEBOOST1;
        PlaySound(audio_bonk);
    }

    Vector2 next = Vector2Add(hro.pos, Vector2Scale(hro.mom, hro.sp));
    if (isroad((int)next.x, (int)next.y)) {
        hro.pos = next;
    } else {
        hro.sp = HRO_COLBOUNCE * SGN(hro.sp);
    }

    hro.ang = Vector2Angle(hro.mom, hro.to);

    SetMusicVolume(audio_drift3, rangefalloff((float)hro.touched, HRO_TIMETOUCH3,         9999.0, 0.3));
    SetMusicVolume(audio_drift2, rangefalloff((float)hro.touched, HRO_TIMETOUCH2, HRO_TIMETOUCH3, 0.3));
    SetMusicVolume(audio_drift1, rangefalloff((float)hro.touched, HRO_TIMETOUCH1, HRO_TIMETOUCH2, 0.3));
    SetMusicVolume(audio_drift0, rangefalloff((float)hro.touched,              1, HRO_TIMETOUCH2, 1));
    SetMusicPitch(audio_bigrev, MAX(hro.sp*1.5, 0.2));

    UpdateMusicStream(audio_bigrev);
    UpdateMusicStream(audio_drift0);
    UpdateMusicStream(audio_drift1);
    UpdateMusicStream(audio_drift2);
    UpdateMusicStream(audio_drift3);

    
}

void
hrosndinit() {
    PlayMusicStream(audio_bigrev);
    SetMusicVolume(audio_bigrev, 0.4);
    PlayMusicStream(audio_drift0);
    PlayMusicStream(audio_drift1);
    PlayMusicStream(audio_drift2);
    PlayMusicStream(audio_drift3);
}

void
doenemies() {
    for(int i = 0; i < enemycount; i++) {
        if(!enemies[i].triggered) {
            if(Vector2DistanceSqr(hro.pos, enemies[i].trigger) < TRIGGERRANGE * TRIGGERRANGE) {
                enemies[i].triggered = true;
                enemies[i].timer = enemies[i].delay;
            }
        }

        if (enemies[i].triggered && !enemies[i].gone) {
            
            enemies[i].timer--;
            if(sin(enemies[i].timer / 8) > 0){
                drawbillboard(Vector3Add(cam.pos, Vector3Add(Vector3Scale(cam.frwd, 8), Vector3Scale(cam.side, enemies[i].road))), sprite_warning, 16);
            }
            float zpos = 15;
            if(enemies[i].timer < 30) {
                zpos = enemies[i].timer/2;
            }
            
            switch(enemies[i].type) {
                case SPEAR:
                    drawbillboard((Vector3) {enemies[i].pos.x, enemies[i].pos.y, zpos}, sprite_speardemon, 4);
                    break;
                case THWOMP:
                    drawbillboard((Vector3) {enemies[i].pos.x, enemies[i].pos.y, zpos}, sprite_fleshthwomp, 3);
                    break;
                default:
                    break;
            }

            if(enemies[i].timer <= 4) {
                float enemyRange = 0;
                switch(enemies[i].type) {
                    case SPEAR:  enemyRange = 8; break;
                    case THWOMP: enemyRange = 16;    break;
                    default:     enemyRange = 2;    break;
                }


                if(Vector2Distance(hro.pos, enemies[i].pos) < enemyRange) {
                    hro.sp = 0;
                    hro.penalty = 30;
                    PlaySound(audio_bonk);
                }
                enemies[i].gone = true;
            }
        }
    }
}

int startframe = 0;

void
donumbers() {
    int numx = 310;
    int numy = 4;

    int framecount = frame - startframe;
    int frac = (framecount % 60) * 100 / 60;
    int sec = framecount / 60;

    for(int x = 0; x < 8; x++) {
        for(int y = 0; y < 16; y++) {
            billpix[numy + y][numx + x] = sprite_numbers.p[y * 96 + x + 8 * (frac % 10)];
        }
    }
    numx -= 10;
    for(int x = 0; x < 8; x++) {
        for(int y = 0; y < 16; y++) {
            billpix[numy + y][numx + x] = sprite_numbers.p[y * 96 + x + 8 * (frac / 10)];
        }
    }
    numx -= 3;
    for(int x = 0; x < 2; x++) {
        for(int y = 0; y < 16; y++) {
            billpix[numy + y][numx + x] = sprite_numbers.p[y * 96 + x + 8 * (10) + 3];
        }
    }
    numx -= 9;
    int min = sec / 60;
    sec = sec % 60;

    for(int x = 0; x < 8; x++) {
        for(int y = 0; y < 16; y++) {
            billpix[numy + y][numx + x] = sprite_numbers.p[y * 96 + x + 8 * (sec % 10)];
        }
    }
    numx -= 10;
    for(int x = 0; x < 8; x++) {
        for(int y = 0; y < 16; y++) {
            billpix[numy + y][numx + x] = sprite_numbers.p[y * 96 + x + 8 * (sec / 10)];
        }
    }
    numx -= 3;
    for(int x = 0; x < 2; x++) {
        for(int y = 0; y < 16; y++) {
            billpix[numy + y][numx + x] = sprite_numbers.p[y * 96 + x + 8 * (11) + 3];
        }
    }
    numx -= 9;
    if(min > 0) {
        for(int x = 0; x < 8; x++) {
            for(int y = 0; y < 16; y++) {
                billpix[numy + y][numx + x] = sprite_numbers.p[y * 96 + x + 8 * (min % 10)];
            }
        }
        numx -= 10;
    }
    if(min >= 10) {
        for(int x = 0; x < 8; x++) {
            for(int y = 0; y < 16; y++) {
                billpix[numy + y][numx + x] = sprite_numbers.p[y * 96 + x + 8 * (min / 10)];
            }
        }
    }
}

Music *currmusic;
Music *currtrans;
bool startchange = false;

void
updatemusic() {
    float cml = GetMusicTimeLength(*currmusic);

    Music *targtrans = NULL;
    if (currmusic == &audio_m1 && !paused) targtrans = &audio_m1t2;
    if (currmusic == &audio_m2 && hro.pos.x > 800) targtrans = &audio_m2t3;
    if (currmusic == &audio_m3 && hro.pos.x > 1450) targtrans = &audio_m3t4;
    if (currmusic == &audio_m4                    ) targtrans = &audio_m4t5;
    if (currmusic == &audio_m5 && hro.pos.x > 2300) targtrans = &audio_m5t6;
    if (currmusic == &audio_m6                    ) targtrans = &audio_m6t7;

    if (targtrans && !currtrans) {
        float tml = GetMusicTimeLength(*targtrans);

        if (GetMusicTimePlayed(*currmusic) >= (cml - tml)) {
            SetMusicVolume(*currmusic, 0);
            currtrans = targtrans;
            currtrans->looping = false;
            PlayMusicStream(*currtrans);
            SeekMusicStream(*currtrans, GetMusicTimePlayed(*currmusic) - (cml - tml));
        }
    }

    if (currtrans) {
        UpdateMusicStream(*currtrans);
        if (!IsMusicStreamPlaying(*currtrans)) {
            if (currtrans == &audio_m1t2) currmusic = &audio_m2;
            if (currtrans == &audio_m2t3) currmusic = &audio_m3;
            if (currtrans == &audio_m3t4) currmusic = &audio_m4;
            if (currtrans == &audio_m4t5) currmusic = &audio_m5;
            if (currtrans == &audio_m5t6) currmusic = &audio_m6;
            if (currtrans == &audio_m6t7) currmusic = &audio_m7;

            PlayMusicStream(*currmusic);

            currtrans = NULL;
        }
    }
    UpdateMusicStream(*currmusic);
}

typedef struct {
    Vector3 pos;
    Vector3 vel;
} Bptc;

#define ffff 500
Bptc bpcts[ffff];
int nb;
int bosstime = 0;
float y;
float scale = 0.5;

void
doboss() {
    if (hro.pos.x < 2300) return;
    Vector3 bosspos = {0};
    float oldx = bosspos.x;

    bosstime++;

    bosspos.x = hro.pos.x + 10;
    float diffx = bosspos.x - oldx;
    bosspos.y = 560;
    bosspos.y = Lerp(bosspos.y, hro.pos.y, 0.4);
    bosspos.z = 6;

    if (nb < ffff && bosstime % 2 == 0 && IsMusicStreamPlaying(audio_m6))
        bpcts[nb++] = (Bptc) {
            .pos = Vector3Add(Vector3Subtract(cam.pos, Vector3Scale(cam.frwd, 3)), (Vector3) {
                        0,
(float)GetRandomValue(-100,100) / 50,
(float)GetRandomValue(-100,100) / 50 - 1
                    }),
            .vel = Vector3Zero(),
        };

    
    scale = Lerp(scale, 10, 0.001);
    for (int n = 0; n < nb; n++) {
        if (Vector3Length(bpcts[n].vel) < 0.01) {
            Vector3 diff = Vector3Subtract(bpcts[n].pos, bosspos);
            Vector3 norm = Vector3Normalize(Vector3Subtract(bpcts[n].pos, bosspos));
            Vector3 targ = Vector3Add(Vector3Scale(norm, 5), bosspos);

            if (!IsMusicStreamPlaying(audio_m7)) {
                
                printf("%f\n", scale);
                diff.z += (float)GetRandomValue(-100, 100) / 10;
                diff.x += (float)GetRandomValue(-100, 100) / 10;
                diff.y += (float)GetRandomValue(-100, 100) / 10;
                Vector3 norm = Vector3Normalize(diff);
                //printv3(norm);
                Vector3 targ = Vector3Add(Vector3Scale(norm, scale), bosspos);
                bpcts[n].pos = Vector3Lerp(bpcts[n].pos, targ, 0.1);
                // float dnz = Vector2Length((Vector2){norm.x, norm.y}) * SGN(norm.x) * SGN(norm.y);
                // Vector2 norm2 = Vector2Normalize((Vector2){dnz, norm.z});
                // bpcts[n].pos.z -= norm2.x;
                // bpcts[n].pos.x += norm2.y;
                // bpcts[n].pos.y -= norm2.y;
            } else {
                
                Vector3 norm = Vector3Normalize(diff);
                
                //printv3(norm);
                Vector3 targ = Vector3Add(Vector3Scale(norm, 5), bosspos);
                bpcts[n].pos.z += (float)GetRandomValue(-100,100) / 50;
                bpcts[n].pos.x += (float)GetRandomValue(-100,100) / 50 + oldx;
                bpcts[n].pos.y += (float)GetRandomValue(-100,100) / 50;
                bpcts[n].pos = Vector3Lerp(bpcts[n].pos, targ, 0.2);
                }
            drawptc2(bpcts[n].pos, PAL9);
        } else {
            bpcts[n].vel = Vector3Scale(bpcts[n].vel, 0.9);
            bpcts[n].pos = Vector3Add(bpcts[n].pos, bpcts[n].vel);
            drawptc2(bpcts[n].pos, PAL34);
        }
    }

    if (bosstime % 100 > 90 && IsMusicStreamPlaying(audio_m7)) {
        if (bosstime % 100 == 91)
            y = (float)GetRandomValue(-100,100) / 200;
        int b = bosstime % 100 - 90;
        for (int n = b*10; n < MIN(b*10+10, 100); n++) {
            bpcts[n].vel = (Vector3) { -0.4, y, 0 };
        }
    }
    if (IsMusicStreamPlaying(audio_m7)) {
        drawptc3(Vector3Add(bosspos, (Vector3){3, 3, 3}), PAL1, 2);
        drawptc3(Vector3Add(bosspos, (Vector3){3, -3, 3}), PAL1, 2);
    }

    //memset(ptcpix , 255, sizeof(ptcpix ));
}

int 
main(void) {
    SetTraceLogLevel(LOG_ERROR); 

    InitWindow(800, 450, "raylib [core] example - basic window");
    InitAudioDevice();
    //SetMasterVolume(0);

    loadass();
    loadmap();
    hrosndinit();

    SetWindowState(FLAG_WINDOW_RESIZABLE);

    Image img = GenImageColor(GAMEW, GAMEH, BLANK);
    Texture2D grndtex = LoadTextureFromImage(img);
    Texture2D billtex = LoadTextureFromImage(img);
    Texture2D ptctex  = LoadTextureFromImage(img);
    hro.pos = (Vector2){0,32};

    PlayMusicStream(audio_cutscene);
    currmusic = &audio_cutscene;

    //PlayMusicStream(audio_m6);
    //currmusic = &audio_m6;

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        partcount = 0;
        billcount = 0;
        //Clear pixel buffers
        memset(billpix, 0, sizeof(billpix));
        memset(ptcpix , 0, sizeof(ptcpix ));
        memset(grndpix, 0, sizeof(grndpix));
        memset(depth, 0, sizeof(depth));

        if (!paused) dohro();
        docam();
        doboss();

        bool on = false;
        if (frame < 170) {
            drawnotice2(sprite_ArcadeControls);
            drawnotice3(sprite_lefthand, frame % 30 >= 10 ? 1 : 0);
            drawnotice3(sprite_righthand, frame % 30 >= 10 ? 1 : 0);
        } else if (frame < 340) {
            drawnotice2(sprite_ArcadeScreen);
        } else if (frame < 505) {
            drawnotice2(sprite_ArcadeFar);
            drawnotice2(sprite_ArcadeFarbloodless);
        } else if (frame < 633) {
            drawnotice2(sprite_ArcadeFar);
            drawnotice3(sprite_ArcadeFarsplat, -MIN(frame - 505, 2));
        } else {
            on = true;
            if (paused && IsKeyPressed(KEY_SPACE)) {
                startFrame = frame;
                paused = false;
            }
        }
        updatemusic();

        if (cam.pos.z < 10 && IsMusicStreamPlaying(audio_cutscene)) {
            StopMusicStream(audio_cutscene);
            PlayMusicStream(audio_m1);
            currmusic = &audio_m1;
        }

        if (on) {
            donumbers();
            doenemies();

            //draw trees
            int index = 0;
            do{
                drawbillboard((Vector3) {treeloc[index].x, treeloc[index].y, 0}, sprite_eviltree, 8);
                index++;
            }
            while(!Vector2Equals(treeloc[index], Vector2Zero()));
            
            drawground();
            drawhro();

            if (cam.pos.z < 10) {
            if (paused) drawnotice(sprite_pressspace);
            if (hro.pos.x > 280 && hro.pos.x < 400) drawnotice(sprite_controls1);
            if (hro.pos.y > 150 && hro.pos.y < 240 && hro.pos.x < 880) drawnotice(sprite_controls2);
            }
        }

        if (on && frame < 772) 
            drawnotice2(sprite_present);
        UpdateTexture(grndtex, grndpix);
        UpdateTexture(billtex, billpix);
        UpdateTexture(ptctex , ptcpix );

        BeginDrawing();
            ClearBackground(RAYWHITE);

            Rectangle gamerect = (Rectangle){0,0,GAMEW,GAMEH};
            Rectangle winrect  = (Rectangle){0,0,GetScreenWidth(),GetScreenHeight()};
            DrawTexturePro(grndtex, gamerect, winrect, Vector2Zero(), 0, WHITE);
            DrawTexturePro(billtex, gamerect, winrect, Vector2Zero(), 0, WHITE);
            DrawTexturePro(ptctex , gamerect, winrect, Vector2Zero(), 0, WHITE);
        EndDrawing();

        frame++;

        if(true){
            printf("Framerate: %i, Billboards: %i, Particles: %i\n", GetFPS(), billcount, partcount);
        }
    }

    CloseWindow();

    return 0;
}
