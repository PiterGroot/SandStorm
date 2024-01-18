#include "SandStorm.h"

constexpr auto SCREEN_WIDTH = 512;
constexpr auto SCREEN_HEIGHT = 512;

int main()
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "SandStorm Engine"); //create raylib window
    DisableCursor();
    SandStorm* sandStorm = new SandStorm();
    while (!WindowShouldClose())
    {
        float deltaTime = GetFrameTime(); //calculate deltaTime
        sandStorm->Update(deltaTime); //update cell engine
    }

    CloseWindow();
    return 0;
}