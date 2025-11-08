#include "map.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#define popcnt __builtin_popcountll

#define TILESZ  8
#define CHUNKSZ (TILESZ*TILESZ)

typedef uint64_t m8x8;
#define M(x, y) ((m8x8)1 << ((y) * 8 + (x)))

typedef bool Boolchunk[CHUNKSZ][CHUNKSZ];

typedef struct {
    m8x8 road;
    m8x8 edge;
    m8x8 *edgeshapes;
    int nedgeshapes;
} Chunk;

m8x8 *edgebuf;
m8x8 *edgebufat;

static Chunk
chunkcompress(Boolchunk road) {
    Chunk chunk = { 
        .road = 0, 
        .edge = 0, 
        .edgeshapes = edgebufat,
        .nedgeshapes = 0,
    };

    for (int by = 0; by < CHUNKSZ; by += TILESZ)
        for (int bx = 0; bx < CHUNKSZ; bx += TILESZ) {
            m8x8 m = 0;
            for (int sy = 0; sy < TILESZ; sy++)
                for (int sx = 0; sx < TILESZ; sx++) {
                    if (road[by+sy][bx+sx]) m |= M(sx,sy);
                }

            m8x8 or = M(bx/TILESZ,by/TILESZ);
            if      (m == 0xffffffffffffffff) chunk.road |= or;
            else if (m == 0x0000000000000000) {}
            else {
                printf("set an edge @ (%d %d)\n", bx, by);
                chunk.edge |= or;
                *(edgebufat++) = m;
                chunk.nedgeshapes += 1;
            }
        }

    return chunk;
}

int framef = 0;

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

bool
isroad(int x, int y) {
    if (x >= 0 && x < CHUNKSZ && y >= 0 && y < CHUNKSZ) {
        return chunkget(&chunk, x, y);
     }
    return false;
}

void
loadmap() {
    edgebuf = malloc(4096 * sizeof(m8x8));
    edgebufat = edgebuf;

    Boolchunk bc = {0};
    for (int x = 0; x < 64; x++)
        for (int y = 0; y < 64; y++)
            if (x*x + y*y < 4096)
                bc[x][y] = true;

    chunk = chunkcompress(bc);
}

void
markframe() {
    framef++;
}
