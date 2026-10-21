#include "exposition.h"
#include "raylib.h"

int main(void) {
  SetConfigFlags(FLAG_WINDOW_HIGHDPI);

  const int screenWidth = 1024;
  const int screenHeight = 768;

  InitWindow(screenWidth, screenHeight, "Exposition");

  SetTargetFPS(120);

  Exposition app;

  app.addCanva(1080, 1080);

  while (!WindowShouldClose()) {
    app.handleInput();

    BeginDrawing();

    ClearBackground(RAYWHITE);

    app.renderCanvas();
    app.renderUI();

    EndDrawing();
  }

  CloseWindow();

  return 0;
}
