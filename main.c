#include "raylib.h"
#include <stdlib.h>

#define WORLD_W 10
#define WORLD_H 10
#define TILE_SIZE 2.5f
#define WALL_HEIGHT 2.5f

// 0 = empty, 1 = wall
int world[WORLD_H][WORLD_W] = {
    {1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,1},
    {1,0,1,0,1,0,1,0,0,1},
    {1,0,0,0,0,0,1,0,0,1},
    {1,0,1,1,1,0,0,0,0,1},
    {1,0,0,0,0,0,1,1,0,1},
    {1,0,0,1,0,0,0,0,0,1},
    {1,0,0,0,0,1,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1},
};

int main(void)
{
    InitWindow(1200, 800, "Gungale");
    DisableCursor();
    SetTargetFPS(60);

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 2.0f, 1.8f, 6.0f };
    camera.target   = (Vector3){ 2.0f, 1.8f, 0.0f };
    camera.up       = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy     = 65.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_FIRST_PERSON);

        BeginDrawing();
        ClearBackground((Color){ 0, 0, 0, 0 });

        BeginMode3D(camera);

        // ---- FLOOR ----
        DrawPlane(
            (Vector3){ (WORLD_W * TILE_SIZE) * 0.5f, 0.0f, (WORLD_W * TILE_SIZE) * 0.5f },
            (Vector2){ WORLD_W * TILE_SIZE, WORLD_H * TILE_SIZE },
            (Color){ 120, 170, 120, 255 }
        );

        // ---- WORLD ----
        for (int z = 0; z < WORLD_H; z++)
        {
            for (int x = 0; x < WORLD_W; x++)
            {
                if (world[z][x] == 1)
                {
                    Vector3 pos = {
                        x * TILE_SIZE,
                        WALL_HEIGHT / 2.0f,
                        z * TILE_SIZE
                    };

                    DrawCube(
                        pos,
                        TILE_SIZE,
                        WALL_HEIGHT,
                        TILE_SIZE,
                        GRAY
                    );

                    DrawCubeWires(
                        pos,
                        TILE_SIZE,
                        WALL_HEIGHT,
                        TILE_SIZE,
                        DARKGRAY
                    );
                }
            }
        }

        EndMode3D();

        DrawText("Welcome to Gungale", 20, 20, 20, BLACK);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}

