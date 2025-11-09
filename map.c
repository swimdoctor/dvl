#include "util.h"
#include "map.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define popcnt __builtin_popcountll

#define TILESZ  8
#define CHUNKSZ (TILESZ*TILESZ)
#define CHUNKBM 0b111111
#define CHUNKBS 6

typedef uint64_t m8x8;
#define M(x, y) ((m8x8)1 << ((y) * 8 + (x)))

typedef struct { int16_t x, y; } i16vec2;

typedef bool Boolchunk[CHUNKSZ][CHUNKSZ];

typedef struct {
    i16vec2 pos;
    m8x8 road, edge;
    m8x8 *edgeshapes;
} Chunk;

m8x8 *edgeshapebuf, *edgeshapebufat;

static Chunk
chunkcompress(Boolchunk road) {
    Chunk chunk = {0};
    chunk.edgeshapes = edgeshapebufat;

    for (int ty = 0; ty < TILESZ; ty++)
        for (int tx = 0; tx < TILESZ; tx++) {
            m8x8 m = 0;
            for (int py = 0; py < TILESZ; py++)
                for (int px = 0; px < TILESZ; px++) {
                    if (road[ty*TILESZ+py][tx*TILESZ+px]) m |= M(px,py);
                }

            m8x8 or = M(tx,ty);
            if      (m == 0xffffffffffffffff) chunk.road |= or;
            else if (m == 0x0000000000000000) {}
            else {
                chunk.edge |= or; 
                *(edgeshapebufat++) = m;
            }
        }

    return chunk;
}

static bool
chunkget(Chunk *chunk, int cx, int cy) {
    m8x8 bm = M(cx/TILESZ, cy/TILESZ);
    if (chunk->road & bm) return true;
    if (chunk->edge & bm) {
        m8x8 sm = M(cx%TILESZ, cy%TILESZ);
        int edgeidx = popcnt(chunk->edge & (bm-1));
        return (chunk->edgeshapes[edgeidx] & sm) != 0;
    }
    return false;
}

Chunk chunk;

#define DETAIL 0.02

typedef struct {
    unsigned char x, y, a, b, w;
} Bez;

#define Bez0 ((Bez) { 0, 0, 0, 0, 0 })

static void
circle(Boolchunk bc, float cx, float cy, float r, bool set) {
    for (int y = 0; y < CHUNKSZ; y++)
        for (int x = 0; x < CHUNKSZ; x++) {
            float dx = (float)x - cx, dy = (float)y - cy;
            if (dx*dx + dy*dy <= r*r) {
                bc[(int)y][(int)x] = set;
            }
        }
}

static void
bezier(Boolchunk bc, Bez* bn) {
    for (Bez *a = &bn[0], *c = &bn[1]; c->w; a++, c++) {
        float ax = (float)a->x, ay = (float)a->y, aw = (float)a->w,
              cx = (float)c->x, cy = (float)c->y, cw = (float)c->w;
        if (a->a == c->a && a->b == c->b) {
            // points are parallel, do line a -> c
            for (float t = 0; t <= 1.01; t += DETAIL) {
                float nx = ax * (1-t) + cx * t,
                      ny = ay * (1-t) + cy * t,
                      nw = aw * (1-t) + cw * t;
                circle(bc, nx, ny, nw, true);
            }
        } else {
            // points are not parallel, do cubic bezier curve a -> b -> c
            float aa = (float)a->a, ab = (float)a->b,
                  ca = (float)c->a, cb = (float)c->b;
            float aic = -(aa * ax + ab * ay);
            float cic = -(ca * cx + cb * cy);

            // calculating b by finding the intersection of a & c's
            // direction lines
            Vector3 bcr = Vector3CrossProduct((Vector3){ aa, ab, aic }, (Vector3){ ca, cb, cic });
            float bx = bcr.x / bcr.z;
            float by = bcr.y / bcr.z;
            float bw = (aw + cw) / 2;

            // pos is defined by (1-t)^2 * a + 2(1-t)t * b + t^2 * c

            for (float t = 0; t <= 1.01; t += DETAIL) {
                float nx = (1-t) * (1-t) * ax + 2 * (1-t) * t * bx + t * t * cx,
                      ny = (1-t) * (1-t) * ay + 2 * (1-t) * t * by + t * t * cy,
                      nw = (1-t) * (1-t) * aw + 2 * (1-t) * t * bw + t * t * cw;
                circle(bc, (int)nx, (int)ny, (int)nw, true);
            }
        }
    }
}

static uint32_t
hash(i16vec2 pos) {
    uint32_t x = pos.x, y = pos.y;
    
    x = (x | (x << 8)) & 0x00FF00FF;
    x = (x | (x << 4)) & 0x0F0F0F0F;
    x = (x | (x << 2)) & 0x33333333;
    x = (x | (x << 1)) & 0x55555555;
    
    y = (y | (y << 8)) & 0x00FF00FF;
    y = (y | (y << 4)) & 0x0F0F0F0F;
    y = (y | (y << 2)) & 0x33333333;
    y = (y | (y << 1)) & 0x55555555;
    
    return x | (y << 1);
}

#define NCHUNK 4096

Chunk *chunks;

static void
insertchunk(i16vec2 pos, Boolchunk bc) {
    Chunk chunk = chunkcompress(bc);
    chunk.pos = pos;
    uint32_t h = hash(pos) % NCHUNK;
    while (chunks[h].edgeshapes != NULL) h = (h + 1) % NCHUNK;
    chunks[h] = chunk;
}

bool
isroad(int x, int y) {
    i16vec2 cpos = (i16vec2) { x >> CHUNKBS, y >> CHUNKBS };
    int h = hash(cpos) % NCHUNK;
    while (true) {
        if (chunks[h].edgeshapes == NULL) return false;
        if (chunks[h].pos.x == cpos.x && chunks[h].pos.y == cpos.y) {
            return chunkget(&chunks[h], x & CHUNKBM, y & CHUNKBM);
        }
        h = (h + 1) % NCHUNK;
    }
}

#define HORI(X, Y, R) (Bez) { X, Y, 255, 1, R  }
#define HORIN(X, Y, N, R) (Bez) { X, Y, 255, N, R  }
#define VERT(X, Y, R) (Bez) { X, Y, 1, 255, R  }
#define LINEX(Y, R1, R2) \
    HORI( 0, Y, R1), \
    HORI(63, Y, R2)
#define LINEY(X, R1, R2) \
    VERT( X, 0, R1), \
    VERT( X,63, R2)


#define STARTCHUNK(BC) memset(BC, 0, sizeof(Boolchunk));
#define ENDCHUNK(BC,X,Y) insertchunk((i16vec2){X,Y}, BC);

void
loadmap() {
    edgeshapebuf = malloc(4096 * sizeof(m8x8));
    edgeshapebufat = edgeshapebuf;
    chunks = calloc(NCHUNK, sizeof(Chunk));

    Boolchunk bc;
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(32, 8, 16), Bez0, });
    ENDCHUNK(bc, 0, 0);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(32, 16, 8), Bez0, });
    ENDCHUNK(bc, 1, 0);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(32, 8, 16), Bez0, });
    ENDCHUNK(bc, 2, 0);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(32, 16, 8), Bez0, });
    ENDCHUNK(bc, 3, 0);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(32, 8, 16), Bez0, });
    ENDCHUNK(bc, 4, 0);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(32, 16, 8), Bez0, });
    ENDCHUNK(bc, 5, 0);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ HORI(0, 32, 8), HORI(64, 50, 8), Bez0, });
    ENDCHUNK(bc, 6, 0);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ HORIN(0, 50, 10, 8), HORIN(64, 32, 10, 8), Bez0, });
    ENDCHUNK(bc, 7, 0);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ HORI(0, 32, 8), HORI(64, 32, 16), Bez0, });
    ENDCHUNK(bc, 8, 0);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ VERT(0, 32, 16), HORI(64, 64, 16), Bez0, });
    ENDCHUNK(bc, 9, 0);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ VERT(0, 48, 16), HORI(0, 64, 32), Bez0, });
    ENDCHUNK(bc, 10, 0);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEY(0, 32, 24), Bez0, });
    ENDCHUNK(bc, 10, 1);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ HORI(0, 0, 24), VERT(63, 48, 8), Bez0, });
    ENDCHUNK(bc, 10, 2);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 8, 10), Bez0, });
    ENDCHUNK(bc, 11, 2);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ VERT(0, 48, 8), HORI(16, 63, 15), Bez0, });
    ENDCHUNK(bc, 12, 2);


#define R1 14
#define R2 10
#define R3 7
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ HORI(16, 0, 15), VERT(0, 16, R1), Bez0, });
        bezier(bc, (Bez[]){ HORI(16, 64, R1), VERT(0, 48, R1), Bez0, });
    ENDCHUNK(bc, 12, 3);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(16, 8, R1), Bez0, });
        bezier(bc, (Bez[]){ LINEX(48, 8, R2), Bez0, });
    ENDCHUNK(bc, 11, 3);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ VERT(63, 16, R1), HORI(48, 32, R2), VERT(63, 48, R2), Bez0, });
    ENDCHUNK(bc, 10, 3);

    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ HORI(16, 0, R2), VERT(0, 16, R2), Bez0, });
        bezier(bc, (Bez[]){ HORI(16, 64, R2), VERT(0, 48, R2), Bez0, });
    ENDCHUNK(bc, 12, 4);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(16, 8, R2), Bez0, });
        bezier(bc, (Bez[]){ LINEX(48, 8, R2), Bez0, });
    ENDCHUNK(bc, 11, 4);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ VERT(63, 16, R2), HORI(48, 32, R3), VERT(63, 48, R3), Bez0, });
    ENDCHUNK(bc, 10, 4);

    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ HORI(16, 0, R3), VERT(0, 16, R3), Bez0, });
        bezier(bc, (Bez[]){ LINEX(48, 8, 16), Bez0, });
    ENDCHUNK(bc, 12, 5);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(16, 8, R3), Bez0, });
        bezier(bc, (Bez[]){ LINEX(48, 8, R3), Bez0, });
    ENDCHUNK(bc, 11, 5);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ VERT(63, 16, R3), HORI(48, 32, 8), VERT(63, 48, 8), Bez0, });
    ENDCHUNK(bc, 10, 5);

    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0, });
    ENDCHUNK(bc, 13, 5);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0, });
    ENDCHUNK(bc, 14, 5);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0, });
    ENDCHUNK(bc, 15, 5);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0, });
    ENDCHUNK(bc, 16, 5);

    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0, });
    ENDCHUNK(bc, 17, 5);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0, });
    ENDCHUNK(bc, 18, 5);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0, });
    ENDCHUNK(bc, 19, 5);

    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0, });
    ENDCHUNK(bc, 20, 5);

    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ VERT(0, 48, 16), HORI(32, 0, 16), Bez0, });
    ENDCHUNK(bc, 21, 5);

    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ VERT(63, 16, 16), HORI(32, 64, 16), Bez0, });
    ENDCHUNK(bc, 21, 4);

    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ VERT(0, 16, 16), HORI(32, 64, 16), Bez0, });
    ENDCHUNK(bc, 22, 4);

    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEY(32, 16, 16), Bez0 });
    ENDCHUNK(bc, 22, 5);

    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ (Bez){ 32, 0, 32, 16, 16 }, (Bez){ 32, 48, 16, -32, 8 }, (Bez){ 64, 48, 16, -32, 16 }, Bez0 });
    ENDCHUNK(bc, 22, 6);

    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0 });
    ENDCHUNK(bc, 23, 6);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0 });
    ENDCHUNK(bc, 24, 6);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0 });
    ENDCHUNK(bc, 25, 6);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0 });
    ENDCHUNK(bc, 26, 6);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0 });
    ENDCHUNK(bc, 27, 6);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0 });
    ENDCHUNK(bc, 28, 6);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0 });
    ENDCHUNK(bc, 29, 6);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0 });
    ENDCHUNK(bc, 29, 6);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0 });
    ENDCHUNK(bc, 30, 6);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(48, 16, 16), Bez0 });
    ENDCHUNK(bc, 31, 6);

    const int OX = 20;
    const int OY = 3;
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ HORI(16, 0, 15), VERT(0, 16, R1), Bez0, });
        bezier(bc, (Bez[]){ HORI(16, 64, R1), VERT(0, 48, R1), Bez0, });
    ENDCHUNK(bc, 12+OX, 3+OY);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(16, 8, R1), Bez0, });
        bezier(bc, (Bez[]){ LINEX(48, 8, R2), Bez0, });
    ENDCHUNK(bc, 11+OX, 3+OY);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ VERT(63, 16, R1), HORI(48, 32, R2), VERT(63, 48, R2), Bez0, });
    ENDCHUNK(bc, 10+OX, 3+OY);

    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ HORI(16, 0, R2), VERT(0, 16, R2), Bez0, });
        bezier(bc, (Bez[]){ HORI(16, 64, R2), VERT(0, 48, R2), Bez0, });
    ENDCHUNK(bc, 12+OX, 4+OY);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(16, 8, R2), Bez0, });
        bezier(bc, (Bez[]){ LINEX(48, 8, R2), Bez0, });
    ENDCHUNK(bc, 11+OX, 4+OY);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ VERT(63, 16, R2), HORI(48, 32, R3), VERT(63, 48, R3), Bez0, });
    ENDCHUNK(bc, 10+OX, 4+OY);

    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ HORI(16, 0, R3), VERT(0, 16, R3), Bez0, });
        bezier(bc, (Bez[]){ LINEX(48, 8, 16), Bez0, });
    ENDCHUNK(bc, 12+OX, 5+OY);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ LINEX(16, 8, R3), Bez0, });
        bezier(bc, (Bez[]){ LINEX(48, 8, R3), Bez0, });
    ENDCHUNK(bc, 11+OX, 5+OY);
    STARTCHUNK(bc);
        bezier(bc, (Bez[]){ VERT(63, 16, R3), HORI(48, 32, 8), VERT(63, 48, 8), Bez0, });
    ENDCHUNK(bc, 10+OX, 5+OY);

    for (int i = 0; i < 3; i++) {
        STARTCHUNK(bc);
            bezier(bc, (Bez[]){ LINEX(48, 12, 12), Bez0 });
            circle(bc, 16, 40, 3, false);
            circle(bc, 32, 48, 3, false);
            circle(bc, 48, 56, 3, false);
        ENDCHUNK(bc, 32+i, 8);
    }

    for (int i = 0; i < 40; i++) {
        STARTCHUNK(bc);
            bezier(bc, (Bez[]){ LINEX(48, 12, 12), Bez0 });
        ENDCHUNK(bc, 35+i, 8);
    }

    //STARTCHUNK(bc);
    //    bezier(bc, (Bez[]){ LINEX(48, 12, 12), Bez0, });
    //ENDCHUNK(bc, 13, 5);

    //STARTCHUNK(bc);
    //    bezier(bc, (Bez[]){ LINEX(32, 8, 8), Bez0, });
    //ENDCHUNK(bc, 7, 0);
    //STARTCHUNK(bc);
    //    bezier(bc, (Bez[]){ LINEX(32, 8, 8), Bez0, });
    //ENDCHUNK(bc, 8, 0);
}
