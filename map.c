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
circle(Boolchunk bc, float cx, float cy, float r) {
    float minx = MAX(cx - r, 0), maxx = MIN(cx + r, CHUNKSZ-1),
          miny = MAX(cy - r, 0), maxy = MIN(cy + r, CHUNKSZ-1);
    for (float y = miny; y <= maxy; y++) {
        for (float x = minx; x <= maxx; x++) {
            float dx = x - cx, dy = y - cy;
            if (dx*dx + dy*dy < r*r) {
                bc[(int)y][(int)x] = true;
            }
        }
    }
}

static void
bezier(Boolchunk bc, Bez* bn) {
    for (Bez *a = &bn[0], *c = &bn[1]; c->w != 0; a++, c++) {
        float ax = (float)a->x, ay = (float)a->y, aw = (float)a->w,
              cx = (float)c->x, cy = (float)c->y, cw = (float)c->w;
        if (a->a == c->a && a->b == c->b) {
            // points are parallel, do line a -> c
            for (float t = 0; t <= 1.01; t += DETAIL) {
                float nx = ax * (1-t) + cx * t,
                      ny = ay * (1-t) + cy * t,
                      nw = aw * (1-t) + cw * t;
                circle(bc, nx, ny, nw);
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
                circle(bc, (int)nx, (int)ny, (int)nw);
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
        if (chunks[h].pos.x == cpos.x && chunks[h].pos.y == cpos.y)
            return chunkget(&chunks[h], x & CHUNKBM, y & CHUNKBM);
        h = (h + 1) % NCHUNK;
    }
}

void
loadmap() {
    edgeshapebuf = malloc(4096 * sizeof(m8x8));
    edgeshapebufat = edgeshapebuf;
    chunks = calloc(NCHUNK, sizeof(Chunk));

    Boolchunk bc;
    memset(bc, 0, sizeof(Boolchunk));
    bezier(bc, (Bez[]){ (Bez) { 3, 1, 255, 1, 8  }, (Bez) { 64, 50, 1, 255, 8 }, Bez0, });
    insertchunk((i16vec2){0,0}, bc);
    memset(bc, 0, sizeof(Boolchunk));
    bezier(bc, (Bez[]){ (Bez) { 0, 50, 255, 1, 6  }, (Bez) { 15, 64, 1, 255, 8 }, (Bez){15,0,255,1,16}, Bez0, });
    insertchunk((i16vec2){1,0}, bc);
    memset(bc, 0, sizeof(Boolchunk));
    bezier(bc, (Bez[]){ (Bez) { 0, 64, 255, 1, 8  }, (Bez) { 64, 0, 1, 255, 16 }, Bez0, });
    insertchunk((i16vec2){0,-1}, bc);
    memset(bc, 0, sizeof(Boolchunk));
    bezier(bc, (Bez[]){ (Bez) { 0, 0, 1, 255, 6  }, (Bez) { 15, 64, 255, 1, 16 }, Bez0, });
    insertchunk((i16vec2){1,-1}, bc);
}
