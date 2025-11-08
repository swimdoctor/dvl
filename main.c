#include "raylib.h"
#include <stdlib.h>

const int
gamew = 320,
gameh = 180;

int 
main(void) {
    InitWindow(800, 450, "raylib [core] example - basic window");

    Image img = GenImageColor(gamew, gameh, PINK);
    Texture2D tex = LoadTextureFromImage(img);

    Color *pix = malloc(gamew * gameh * sizeof(Color));
    for (int x = 0; x < gamew; x++)
        for (int y = 0; y < gameh; y++)
            pix[y * gamew + x] = (Color) { x, y, 0, 255 };

    UpdateTexture(tex, pix);

    while (!WindowShouldClose())
    {
        BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawTexturePro(tex, 
                (Rectangle){0,0,320,180}, (Rectangle){0,0,800,450}, 
                (Vector2){0,0}, 0.0,
                WHITE
                );
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
