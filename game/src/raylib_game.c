/*******************************************************************************************
*
*   raylib game template
*
*   <Game title>
*   <Game description>
*
*   This game has been created using raylib (www.raylib.com)
*   raylib is licensed under an unmodified zlib/libpng license (View raylib.h for details)
*
*   Copyright (c) 2021 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include "screens.h"    // NOTE: Declares global (extern) variables and screens functions
#include "stdlib.h"
#include "stdio.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

#define WIDTH 512
#define HEIGHT 512

typedef enum ParticleType {
    SOLID,
    LIQUID,
    GAS,
} ParticleType;

typedef struct particle {
    unsigned int id;
    float life_time;
    Vector2 velocity;
    Color color;
    bool has_been_updated;
    ParticleType type;
} particle;

//----------------------------------------------------------------------------------
// Shared Variables Definition (global)
// NOTE: Those variables are shared between modules through screens.h
//----------------------------------------------------------------------------------
GameScreen currentScreen = LOGO;
Font font = { 0 };
Music music = { 0 };
Sound fxCoin = { 0 };

//----------------------------------------------------------------------------------
// Local Variables Definition (local to this module)
//----------------------------------------------------------------------------------
static const int screenWidth = 800;
static const int screenHeight = 512;

// Grid for storing particles
particle* grid[HEIGHT][WIDTH] = { NULL };

float gravity = 10.0;

// Required variables to manage screen transitions (fade-in, fade-out)
static float transAlpha = 0.0f;
static bool onTransition = false;
static bool transFadeOut = false;
static int transFromScreen = -1;
static GameScreen transToScreen = UNKNOWN;

//----------------------------------------------------------------------------------
// Local Functions Declaration
//----------------------------------------------------------------------------------
static void ChangeToScreen(int screen);     // Change to screen, no transition effect

static void TransitionToScreen(int screen); // Request transition to next screen
static void UpdateTransition(void);         // Update transition effect
static void DrawTransition(void);           // Draw transition effect (full-screen rectangle)

static void UpdateDrawFrame(void);          // Update and draw one frame

static float MinFloat(float a, float b);
static float Clamp(float v, float min, float max);
static Vector2 Vector2Clamp(Vector2 value, Vector2 min, Vector2 max);

static void UpdateSandParticle(particle* grid[HEIGHT][WIDTH], int x, int y);
static void UpdateWaterParticle(particle* grid[HEIGHT][WIDTH], int x, int y);
static particle* CreateParticle(int id, ParticleType type);
static void SwapParticles(particle* grid[HEIGHT][WIDTH], int x1, int y1, int x2, int y2);
static void MoveParticle(particle* grid[HEIGHT][WIDTH], int x1, int y1, int x2, int y2);
static void TranslateParticle(particle* grid[HEIGHT][WIDTH], int x, int y, int stepsX, int stepsY);

//----------------------------------------------------------------------------------
// Main entry point
//----------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //---------------------------------------------------------
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "raylib game template");
    SetWindowMinSize(WIDTH, HEIGHT);

    RenderTexture2D target = LoadRenderTexture(WIDTH, HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    InitAudioDevice();      // Initialize audio device

    // Load global data (assets that must be available in all screens, i.e. font)
    font = LoadFont("resources/mecha.png");
    music = LoadMusicStream("resources/ambient.ogg");
    fxCoin = LoadSound("resources/coin.wav");

    SetMusicVolume(music, 1.0f);
    PlayMusicStream(music);

    // Setup and init first screen
    //currentScreen = LOGO;
    //InitLogoScreen();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else
    SetTargetFPS(60);       // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        float scale = MinFloat((float)GetScreenWidth() / WIDTH, (float)GetScreenHeight() / HEIGHT);

		int maxX = GetScreenWidth();
		int maxY = GetScreenHeight();
		// Insert a sand particle wherever the mouse is pressed
		Vector2 mouse = GetMousePosition();
		int x = (mouse.x / maxX) * WIDTH;
		int y = (mouse.y / maxY) * HEIGHT;
		if (IsKeyDown(KEY_S)) {
			for (int i = -15; i < 16; i++) {
				for (int j = -15; j < 16; j++) {
                    if (abs(i) + abs(j) < 20) {
						if (x + i >= 0 && x + i < WIDTH && y + j >= 0 && y + j < HEIGHT && grid[y + j][x + i] == NULL) {
							if ((x + i + j) % 7 == 0 && (y + j) % 5 == 0) {
								grid[y + j][x + i] = CreateParticle(1, SOLID);
							}
						}
                    }
				}
			}
		}
		else if (IsKeyDown(KEY_W)) {
			for (int i = -15; i < 16; i++) {
				for (int j = -15; j < 16; j++) {
                    if (abs(i) + abs(j) < 20) {
						if (x + i >= 0 && x + i < WIDTH && y + j >= 0 && y + j < HEIGHT && grid[y + j][x + i] == NULL) {
							if ((x + i + j) % 7 == 0 && (y + j) % 5 == 0) {
								grid[y + j][x + i] = CreateParticle(2, LIQUID);
							}
						}
                    }
				}
			}
		}

		for (int x = 0; x < WIDTH; x++) {
			for (int y = HEIGHT - 1; y > 0; y--) {
				if (grid[y][x] != NULL && !grid[y][x]->has_been_updated) {
                    switch (grid[y][x]->id) {
                    case 1:
                        UpdateSandParticle(grid, x, y);
                        break;
                    case 2:
                        UpdateWaterParticle(grid, x, y);
                        break;
                    }
				}
			}
		}

        // Draw
        //----------------------------------------------------------------------------------
        // Draw everything in the render texture, note this will not be rendered on screen, yet
        BeginTextureMode(target);
            ClearBackground(BLACK);  // Clear render texture background color

            for (int x = 0; x < WIDTH; x++) {
                for (int y = 0; y < HEIGHT; y++) {
                    if (grid[y][x] != NULL) {
						switch (grid[y][x]->id) {
						case 1:
                            if (grid[y][x]->has_been_updated) {
                                DrawPixel(x, y, GOLD);
                            }
                            else {
                                DrawPixel(x, y, ORANGE);
                            }
							break;
						case 2:
                            if (grid[y][x]->has_been_updated) {
                                DrawPixel(x, y, BLUE);
                            }
                            else {
                                DrawPixel(x, y, DARKBLUE);
                            }
							break;
						}
                    }
                }
            }
        EndTextureMode();
        
        BeginDrawing();
            ClearBackground(BLACK);     // Clear screen background

            // Draw render texture to screen, properly scaled
            DrawTexturePro(target.texture, (Rectangle) { 0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height },
                (Rectangle) {
                0, 1,
                GetScreenWidth(), GetScreenHeight()-3
            }, (Vector2) { 0, 0 }, 0.0f, WHITE);
        EndDrawing();

		for (int x = 0; x < WIDTH; x++) {
			for (int y = 0; y < HEIGHT; y++) {
				if (grid[y][x] != NULL && grid[y][x]->has_been_updated) {
                    grid[y][x]->has_been_updated = false;
				}
			}
		}
    }
#endif

    // De-Initialization
    //--------------------------------------------------------------------------------------
    //
    UnloadRenderTexture(target);

    // Unload global data loaded
    UnloadFont(font);
    UnloadMusicStream(music);
    UnloadSound(fxCoin);

    CloseAudioDevice();     // Close audio context

    CloseWindow();          // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
static float Clamp(float v, float min, float max) {
    if (v < min) 
    {
        return min;
    }
    else if (v > max)
    {
        return max;
    }
    return v;
}

static particle* CreateParticle(int id, ParticleType type) {
    particle* newParticle = (particle*) malloc(sizeof(particle));
    if (newParticle == NULL) {
        perror("Failed to allocate memory for new particle");
        exit(1);
    }
    newParticle->id = id;
    newParticle->type = type;
    newParticle->has_been_updated = false;
    newParticle->color = ORANGE;
    newParticle->velocity.x = 0.0f;
    newParticle->velocity.y = 0.0f;
    return newParticle;
}

static void SwapParticles(particle* grid[HEIGHT][WIDTH], int x1, int y1, int x2, int y2) {
	grid[y2][x2]->has_been_updated = true;

	particle* tmp = grid[y2][x2];
	grid[y2][x2] = grid[y1][x1];
	grid[y1][x1] = tmp;
}

static void MoveParticle(particle* grid[HEIGHT][WIDTH], int x1, int y1, int x2, int y2) {
	grid[y1][x1]->has_been_updated = true;
	grid[y2][x2] = grid[y1][x1];
	grid[y1][x1] = NULL;
}

static void TranslateParticle(particle* grid[HEIGHT][WIDTH], int x, int y, int stepsX, int stepsY) {
        // Calculate direction of each step
    int dx = (stepsX > 0) ? 1 : (stepsX < 0) ? -1 : 0;
    int dy = (stepsY > 0) ? 1 : (stepsY < 0) ? -1 : 0;

    int currentX = x;
    int currentY = y;

    // Perform movement in x-direction
    for (int i = 0; i < abs(stepsX); i++) {
        int nextX = currentX + dx;
        // Check if out of bounds or if the next cell is occupied
        if (nextX < 0 || nextX >= WIDTH || grid[currentY][nextX] != NULL) {
            break;  // Stop the movement if out of bounds or blocked
        }
        grid[currentY][nextX] = grid[currentY][currentX];
        grid[currentY][currentX] = NULL;
        currentX = nextX;
    }

    // Perform movement in y-direction
    for (int j = 0; j < abs(stepsY); j++) {
        int nextY = currentY + dy;
        // Check if out of bounds or if the next cell is occupied
        if (nextY < 0 || nextY >= HEIGHT || grid[nextY][currentX] != NULL) {
            break;  // Stop the movement if out of bounds or blocked
        }
        grid[nextY][currentX] = grid[currentY][currentX];
        grid[currentY][currentX] = NULL;
        currentY = nextY;
    }
}

static void UpdateSandParticle(particle* grid[HEIGHT][WIDTH], int x, int y) {
    if (grid[y][x] == NULL) return;

    float dt = GetFrameTime();

    grid[y][x]->velocity.y = Clamp(grid[y][x]->velocity.y + (gravity * dt), -10.0, 10.0);
    int vy = grid[y][x]->velocity.y;

    // Down
    if (grid[y + 1][x] == NULL) {
	    grid[y][x]->has_been_updated = true;
        TranslateParticle(grid, x, y, 0, vy);
    }
    else if (grid[y + 1][x]->id == 2 && !grid[y+1][x]->has_been_updated) {
	    grid[y][x]->has_been_updated = true;
        SwapParticles(grid, x, y, x, y + 1);
    }
    // Down left
    else if (grid[y + 1][x-1] == NULL) {
	    grid[y][x]->has_been_updated = true;
        TranslateParticle(grid, x, y, -1, vy);
    }
    else if (grid[y + 1][x-1]->id == 2 && !grid[y+1][x-1]->has_been_updated) {
	    grid[y][x]->has_been_updated = true;
        SwapParticles(grid, x, y, x - 1, y + 1);
    }
    // Down right
    else if (grid[y + 1][x+1] == NULL) {
	    grid[y][x]->has_been_updated = true;
        TranslateParticle(grid, x, y, 1, vy);
    }
    else if (grid[y + 1][x+1]->id == 2 && !grid[y+1][x+1]->has_been_updated) {
	    grid[y][x]->has_been_updated = true;
        SwapParticles(grid, x, y, x+1, y + 1);
    }
    else {
        grid[y][x]->velocity.y /= 2.0;
    }
}
static void UpdateWaterParticle(particle* grid[HEIGHT][WIDTH], int x, int y) {
    if (grid[y][x] == NULL) return;

	if (grid[y + 1][x] == NULL) {
	    grid[y][x]->has_been_updated = true;
        TranslateParticle(grid, x, y, 0, 1);
	}
	else if (x > 0 && grid[y + 1][x - 1] == NULL) {
	    grid[y][x]->has_been_updated = true;
        TranslateParticle(grid, x, y, -1, 1);
	}
	else if (x < WIDTH - 1 && grid[y + 1][x + 1] == NULL) {
	    grid[y][x]->has_been_updated = true;
        TranslateParticle(grid, x, y, 1, 1);
    }
    else if (x - 1 > 0 && grid[y][x - 1] == NULL) {
	    grid[y][x]->has_been_updated = true;
        TranslateParticle(grid, x, y, -5, 0);
    }
    else if (x + 1 < WIDTH - 1 && grid[y][x + 1] == NULL) {
	    grid[y][x]->has_been_updated = true;
        TranslateParticle(grid, x, y, 5, 0);
    }
}

static Vector2 Vector2Clamp(Vector2 value, Vector2 min, Vector2 max) {
    Vector2 result;
    result.x = (value.x < min.x) ? min.x : (value.x > max.x) ? max.x : value.x;
    result.y = (value.y < min.y) ? min.y : (value.y > max.y) ? max.y : value.y;
    return result;
}

static float MinFloat(float a, float b) {
    if (a < b) {
        return a;
    }
    return b;
}
