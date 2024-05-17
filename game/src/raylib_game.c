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
#include "raymath.h"
#include "screens.h"    // NOTE: Declares global (extern) variables and screens functions
#include "stdlib.h"
#include "stdio.h"

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
    #define GLSL_VERSION            100
#endif

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#endif


#define WIDTH 256
#define HEIGHT 256

typedef struct Vector2Int {
    int x;
    int y;
} Vector2Int;

typedef enum particle_state_t {
    SOLID_STUCK,
    SOLID,
    LIQUID,
    GAS,
} particle_state_t;

typedef enum particle_mat_t {
    NOTHING,
    SAND,
    WATER,
    SMOKE,
    WOOD,
    LAVA,
    STONE,
    FIRE,
    OIL,
} particle_mat_t;

typedef struct mat_prop_t {
    float modX;
    float modY;
    float maxX;
    float maxY;
    int flammableProbability;
    float initLifeTime;
    bool decaying;
    bool acting;
    bool flammable;
    particle_state_t type;
    Color initialColor;
} mat_prop_t;

static mat_prop_t props[9] = {
    {0, 0, 0, 0, 0, 0, false, false, false, SOLID_STUCK, {0, 0, 0, 255}}, // Nothing
    {2, 2, 2, 10, 0, 0, false, false, false, SOLID, {140, 103, 50, 255}}, // Sand
    {2, 2, 10, 10, 50, 0, false, false, true, LIQUID, {0, 121, 241, 255}}, // Water
    {2, 2, 5, 10, 0, 5, true, false, false, GAS, {60, 60, 60, 255}}, // Smoke
    {0, 0, 0, 0, 10, 0, false, false, true, SOLID_STUCK, {76, 63, 47, 255}}, // Wood
    {2, 1, 1.5, 4, 0, 0, false, true, false, LIQUID, {255, 101, 32, 255}}, // Lava
    {0, 0, 0, 0, 0, 0, false, false, false, SOLID_STUCK, {100, 100, 100, 255}}, // Stone
    {0, 0, 0, 0, 0, 1, true, true, false, SOLID_STUCK, {255, 180, 10, 255}}, // Fire
    {2, 2, 10, 10, 50, 0, false, false, true, LIQUID, {40, 30, 21, 255}}, // Oil
};

typedef struct particle_t {
    unsigned int id;
    float lifeTime;
    Vector2 velocity;
    Color color;
    bool hasBeenUpdated;
    bool stuck;
    particle_mat_t mat;
} particle_t;

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

float gravity = 10.0;
unsigned int frameCounter = 0;
unsigned int updatedParticles = 0;
unsigned int actuallyUpdatedParticles = 0;

//----------------------------------------------------------------------------------
// Local Functions Declaration
//----------------------------------------------------------------------------------

static float MinFloat(float a, float b);

static void UpdateSolidParticle(particle_t* grid[HEIGHT * WIDTH], int x, int y);
static void UpdateLiquidParticle(particle_t* grid[HEIGHT * WIDTH], int x, int y);
static void UpdateGasParticle(particle_t* grid[HEIGHT * WIDTH], int x, int y);
static void UpdateSolidStuckParticle(particle_t* grid[HEIGHT * WIDTH], int x, int y);

static particle_t* CreateParticle(particle_mat_t mat);
static particle_t* GetParticle(particle_t* grid[HEIGHT * WIDTH], int x, int y);
static int GetIndex(int x, int y);
static void SwapParticles(particle_t* grid[HEIGHT * WIDTH], int x1, int y1, int x2, int y2);
static bool TranslateParticle(particle_t* grid[HEIGHT * WIDTH], int x, int y, int x1, int y1);
static Vector2Int TranslateLiquidParticle(particle_t** grid, int x, int y, int x1, int y1);
static Vector2Int TranslateSolidParticle(particle_t* grid[HEIGHT * WIDTH], int x, int y, int x1, int y1);
static void SpawnParticles(particle_t* grid[HEIGHT * WIDTH], int x, int y, particle_mat_t material);

float isSurroundedByType(particle_t* grid[WIDTH * HEIGHT], int x, int y, particle_mat_t mat);
bool CheckValidMove(particle_t** grid, int i, particle_state_t particleState);
bool withinBounds(int x, int y);

//----------------------------------------------------------------------------------
// Main entry point
//----------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //---------------------------------------------------------
    SetConfigFlags(FLAG_BORDERLESS_WINDOWED_MODE | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 800, "PixelPhysics");
    SetWindowMinSize(1280, 800);

    RenderTexture2D target = LoadRenderTexture(WIDTH, HEIGHT);
    RenderTexture2D noBloom = LoadRenderTexture(WIDTH, HEIGHT);
    RenderTexture2D bloomTarget = LoadRenderTexture(1280, 800); // Texture for bloom shader
 
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT | TEXTURE_WRAP_CLAMP);
    SetTextureFilter(bloomTarget.texture, TEXTURE_WRAP_CLAMP);

    InitAudioDevice();      // Initialize audio device

	// Grid for storing particles
	particle_t** grid2 = (particle_t **) malloc(HEIGHT * WIDTH * sizeof(particle_t *));
    for (int i = 0; i < HEIGHT * WIDTH; i++) {
        grid2[i] = NULL;
    }

    int currentMaterial = SAND;
    char fpsText[100];

    Shader shader = LoadShader(0, TextFormat("resources/bloom.fs", GLSL_VERSION));

    // Setup and init first screen

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else
    SetTargetFPS(60);       // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        frameCounter = (frameCounter + 1) % INT_MAX;
        float scale = MinFloat((float)GetScreenWidth() / WIDTH, (float)GetScreenHeight() / HEIGHT);

		int maxX = GetScreenWidth();
		int maxY = GetScreenHeight();
		// Insert a sand particle wherever the mouse is pressed
		Vector2 mouse = GetMousePosition();
		int x = (mouse.x / maxX) * WIDTH;
		int y = (mouse.y / maxY) * HEIGHT;
		if (IsKeyPressed(KEY_S)) {
            currentMaterial = SAND;
		}
		else if (IsKeyPressed(KEY_W)) {
            currentMaterial = WATER;
		}
		else if (IsKeyPressed(KEY_G)) {
            currentMaterial = SMOKE;
		}
		else if (IsKeyPressed(KEY_E)) {
            currentMaterial = WOOD;
		}
		else if (IsKeyPressed(KEY_A)) {
            currentMaterial = LAVA;
        }
        else if (IsKeyPressed(KEY_Q)) {
            currentMaterial = OIL;
        }
        else if (IsKeyPressed(KEY_Z)) {
            currentMaterial = STONE;
        }
        else if (IsKeyPressed(KEY_R)) {
            currentMaterial = FIRE;
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            SpawnParticles(grid2, x, y, currentMaterial);
        }

        int start, end, step;
        if (frameCounter % 2 == 0) { // Left to right on even frames
			start = 0;
			end = WIDTH;
			step = 1;
		} else { // Right to left on odd frames
			start = WIDTH - 1;
			end = -1;
			step = -1;
		}

		for (int x = start; x != end; x += step) {
			for (int y = HEIGHT - 1; y >= 0; y--) {
                particle_t* p = GetParticle(grid2, x, y);
				if (p != NULL) {
                    switch (props[p->mat].type) {
                    case SOLID:
                        UpdateSolidParticle(grid2, x, y);
                        break;
                    case LIQUID:
                        UpdateLiquidParticle(grid2, x, y);
                        break;
                    case GAS:
                        UpdateGasParticle(grid2, x, y);
                        break;
                    case SOLID_STUCK:
                        UpdateSolidStuckParticle(grid2, x, y);
                        break;
                    }
				}
			}
		}

        // Draw
        //----------------------------------------------------------------------------------
        // Draw everything in the render texture, note this will not be rendered on screen, yet

        BeginTextureMode(target);
			ClearBackground(BLACK);
            int j = 0;
            for (int i = 0; i < WIDTH * HEIGHT - 1; i++) {
                if (i % WIDTH == 0) {
                    j++;
                }
				particle_t* p = grid2[i];
				if (p != NULL) {
				    p->hasBeenUpdated = false;

					if (p->stuck) {
						DrawPixel(i % WIDTH, j, GREEN);
					}
					else {
						if (p->velocity.x > 0) {
							DrawPixel(i % WIDTH, j, RED);
						}
						else if (p->velocity.x < 0) {
							DrawPixel(i % WIDTH, j, BLUE);
						}
						else {
							DrawPixel(i % WIDTH, j, WHITE);
						}
					}
                }
            }
        EndTextureMode();

        BeginTextureMode(bloomTarget);
			BeginShaderMode(shader);

                //DrawTextureRec(target.texture, (Rectangle){ 0, 0, (float)target.texture.width, (float)-target.texture.height }, (Vector2){ 0, 0 }, WHITE);
				DrawTexturePro(target.texture,
				   (Rectangle){0, 0, target.texture.width, target.texture.height}, // Use correct dimensions
				   (Rectangle){0, 0, 1280, 800}, // Ensure it covers the full screen
				   (Vector2){0, 0}, 0.0f, WHITE);
			EndShaderMode();
		EndTextureMode();
        
        int fps = GetFPS();
        sprintf(&fpsText, "%d - %d p - %d u\0", fps, updatedParticles, actuallyUpdatedParticles);
        BeginDrawing();
            ClearBackground(BLACK);     // Clear screen background
            /*
			DrawTexturePro(bloomTarget.texture,
						   (Rectangle){0, 0, bloomTarget.texture.width, bloomTarget.texture.height},
						   (Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()},
						   (Vector2){0, 0}, 0.0f, WHITE);
                           */

			DrawTexturePro(target.texture,
						   (Rectangle){0, 0, target.texture.width, -target.texture.height},
						   (Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()},
						   (Vector2){0, 0}, 0.0f, WHITE);
            DrawText(fpsText, 5, 5, 14, RAYWHITE);
        EndDrawing();

        updatedParticles = 0;
        actuallyUpdatedParticles = 0;
    }
#endif

    // De-Initialization
    //--------------------------------------------------------------------------------------
    //
    UnloadRenderTexture(target);

    UnloadShader(shader);

    CloseAudioDevice();     // Close audio context

    CloseWindow();          // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}


static int GetIndex(int x, int y) {
    return y * WIDTH + x;
}

static particle_t* GetParticle(particle_t* grid[HEIGHT * WIDTH], int x, int y) {
    return grid[y * WIDTH + x];
}

static void SpawnParticles(particle_t* grid[HEIGHT * WIDTH], int x, int y, particle_mat_t mat) {

    if (props[mat].type != SOLID_STUCK) {
	    //grid[y * WIDTH + x] = CreateParticle(mat);
		for (int i = -15; i < 16; i++) {
			for (int j = -15; j < 16; j++) {
				if (abs(i) + abs(j) < 20) {
					if (x + i >= 0 && x + i < WIDTH && y + j >= 0 && y + j < HEIGHT && grid[(y + j) * WIDTH + x + i] == NULL) {
						if ((x + i + j) % 2 == 0 && (y + j) % 2 == 0) {
							grid[(y + j) * WIDTH + x + i] = CreateParticle(mat);
						}
					}
				}
			}
		}
    }
    else {
		for (int i = -10; i < 10; i++) {
			for (int j = -10; j < 10; j++) {
				if (abs(i) + abs(j) < 12) {
					if (x + i >= 0 && x + i < WIDTH && y + j >= 0 && y + j < HEIGHT && grid[(y + j) * WIDTH + x + i] == NULL) {
					    grid[(y + j) * WIDTH + x + i] = CreateParticle(mat);
					}
				}
			}
		}
    }
}

static particle_t* CreateParticle(particle_mat_t material) {
    particle_t* newParticle = (particle_t*) malloc(sizeof(particle_t));
    if (newParticle == NULL) {
        perror("Failed to allocate memory for new particle");
        exit(1);
    }
    newParticle->mat = material;
    if (props[material].decaying) {
        newParticle->lifeTime = props[material].initLifeTime;
    }
    newParticle->hasBeenUpdated = false;
    if ((props[material].type == SOLID_STUCK || props[material].type == SOLID) && material != FIRE) {
        // Scramble colors a bit
        newParticle->color = props[material].initialColor;
        int num = rand();
        newParticle->color.b += (-10 + num % 20);
        newParticle->color.g += (-10 + num % 20);
        newParticle->color.r += (-10 + num % 20);
    }
    else {
        newParticle->color = props[material].initialColor;
    }
    if (props[material].type == LIQUID) {
		newParticle->velocity.x = 0.0f;
		newParticle->velocity.y = 3.0f;
    }
    else {
		newParticle->velocity.x = 0.0f;
		newParticle->velocity.y = 0.0f;
    }
    newParticle->stuck = false;
    return newParticle;
}

static void SwapParticles(particle_t* grid[HEIGHT * WIDTH], int x1, int y1, int x2, int y2) {
    int i = GetIndex(x2, y2);
    int j = GetIndex(x1, y1);
    grid[i]->hasBeenUpdated = true;

    particle_t* tmp = grid[i];
    grid[i] = grid[j];
    grid[j] = tmp;
    tmp->velocity.y = -0.1;
    tmp->velocity.x = (x1 < x2) ? props[tmp->mat].maxX : -props[tmp->mat].maxX;
}

static Vector2Int TranslateSolidParticle(particle_t* grid[HEIGHT * WIDTH], int x, int y, int x1, int y1) {

    int memX = x;
    int memY = y;

    bool moved = false;

    int ax = x;
    int ay = y;

	int dx =  abs (x1 - x), sx = x < x1 ? 1 : -1;
	int dy = -abs (y1 - y), sy = y < y1 ? 1 : -1; 
	int err = dx + dy, e2; /* error value e_xy */

	for (int i = 0; i < 3; i++){  /* loop */

		if (x == x1 && y == y1) break;
		e2 = 2 * err;

		if (e2 >= dy) { 
            err += dy;
            x += sx;
			// If not blocked, continue
			if (x < 0 || x >= WIDTH) {
				break;
			}
            else if (GetParticle(grid, x, y) != NULL) {
                if (props[GetParticle(grid, x, y)->mat].type > SOLID) {
                    // Fall through
                    SwapParticles(grid, ax, y, x, y);
                }
                else if (y + sy >= 0 && y + sy < HEIGHT - 1) {
		            e2 = 2 * err;
					if (e2 <= dx && GetParticle(grid, x, y + sy) == NULL) {
						grid[GetIndex(x, y + sy)] = grid[GetIndex(ax, y)];
						grid[GetIndex(ax, y)] = NULL;

						err += dx;
						y += sy;
						ay = y;
					}
					else if(e2 <= dx && props[grid[GetIndex(x, y+sy)]->mat].type > SOLID) {
						// Check if we can move in y dir before breaking
						SwapParticles(grid, ax, y, x, y + sy);

						err += dx;
						y += sy;
						ay = y;
                    }
                    else {
                        break;
                    }
                }
                else {
                    break;
                }
            }
            else {
				grid[GetIndex(x, y)] = grid[GetIndex(ax, y)];
				grid[GetIndex(ax, y)] = NULL;
            }
			ax = x;
            moved = true;
        } /* e_xy+e_x > 0 */

	    if (e2 <= dx) {
            err += dx;
            y += sy;
			// If not blocked, continue
			if (y < 0 || y >= HEIGHT ) {
                break;
            }
            else if (grid[GetIndex(x, y)] != NULL) {
                if (props[grid[GetIndex(x,y)]->mat].type > SOLID) {
                    // Fall through
                    SwapParticles(grid, x, ay, x, y);
                    moved = true;
                }
                else if (x + sx >= 0 && x + sx < WIDTH - 1) {
		            e2 = 2 * err;
					if (e2 >= dy && grid[GetIndex(x + sx, y)] == NULL) {
						grid[GetIndex(x + sx, y)] = grid[GetIndex(x, ay)];
						grid[GetIndex(x, ay)] = NULL;

						err += dy;
						x += sx;
						ax = x;
					}
					else if(e2 >= dy && props[grid[GetIndex(x + sx, y)]->mat].type > SOLID) {
						// Check if we can move in y dir before breaking
						SwapParticles(grid, x, ay, x+sx, y);

						err += dy;
						x += sx;
						ax = x;
                    }
                    else {
                        break;
                    }
                }
                else {
                    break;
                }
            }
            else {
				grid[GetIndex(x, y)] = grid[GetIndex(x, ay)];
				grid[GetIndex(x, ay)] = NULL;
            }
			ay = y;
			moved = true;

        } /* e_xy+e_y < 0 */
	}
    return (Vector2Int) { x, y };
}

static Vector2Int TranslateLiquidParticle(particle_t** grid, int x0, int y0, int x1, int y1) {

    int x = x0;
    int y = y0;

    int memX = x;
    int memY = y;

    int ax = x;
    int ay = y;

	int dx =  abs (x1 - x), sx = x < x1 ? 1 : -1;
	int dy = -abs (y1 - y), sy = y < y1 ? 1 : -1; 
	int err = dx + dy, e2; /* error value e_xy */

	for (int i = 0; i < 10; i++){  /* loop */

		if (x == x1 && y == y1) break;
		e2 = 2 * err;

		if (e2 >= dy) { 
            err += dy;
            x += sx;
			// If not blocked, continue
			if (x < 0 || x >= WIDTH) {
				break;
			}
            else if (GetParticle(grid, x, y) != NULL) {
                // Liquids should move through gasses
                if (props[GetParticle(grid, x, y)->mat].type == GAS) {
                    // Fall through
                    SwapParticles(grid, ax, y, x, y);
                }
                // This particle was blocked by a horizontal one, try to move diagonally instead
                else if (y + sy >= 0 && y + sy < HEIGHT) {
		            e2 = 2 * err;

					err += dx;
					y += sy;
                    // Check if it is empty
					if (GetParticle(grid, x, y) == NULL) {
						grid[GetIndex(x, y)] = grid[GetIndex(ax, ay)];
						grid[GetIndex(ax, ay)] = NULL;
					}
                    // Check if the particle was a of a gas type
					else if(props[grid[GetIndex(x, y)]->mat].type == GAS) {
						// Check if we can move in y dir before breaking
						SwapParticles(grid, ax, ay, x, y);
                    }
                    else {
                        break;
                    }
				    ay = y;
                }
                else {
                    break;
                }
            }
            else {
				grid[GetIndex(x, y)] = grid[GetIndex(ax, y)];
				grid[GetIndex(ax, y)] = NULL;
            }
			ax = x;
        } /* e_xy+e_x > 0 */

	    if (e2 <= dx) {
            err += dx;
            y += sy;
			// If not blocked, continue
			if (y < 0 || y >= HEIGHT ) {
                break;
            }
            else if (grid[GetIndex(x, y)] != NULL) {
                if (props[grid[GetIndex(x,y)]->mat].type == GAS) {
                    // Fall through
                    SwapParticles(grid, x, ay, x, y);
                }
                else if (x + sx >= 0 && x + sx < WIDTH) {
		            e2 = 2 * err;

					err += dy;
					x += sx;

                    // Try diagonal down
					if (grid[GetIndex(x, y)] == NULL) {
						grid[GetIndex(x, y)] = grid[GetIndex(ax, ay)];
						grid[GetIndex(ax, ay)] = NULL;
						ax = x;
					}
					else if(props[grid[GetIndex(x, y)]->mat].type == GAS) {
						// Check if we can move in x dir before breaking
						SwapParticles(grid, ax, ay, x, y);
						ax = x;
                    }
                    // Try horizontal movement
                    else if (ay != y) {
                        y -= sy;
                        err -= dx;

                        if (grid[GetIndex(x, y)] == NULL) {
							grid[GetIndex(x, y)] = grid[GetIndex(x, y)];
							grid[GetIndex(x, y)] = NULL;
                        }
                        else if (props[grid[GetIndex(x, y)]->mat].type == GAS) {
						    SwapParticles(grid, x, y, x, y);
                        }
                        else {
                            break;
                        }
                    }
                    else {
                        break;
                    }
                }
                else {
                    break;
                }
            }
            else {
				grid[GetIndex(x, y)] = grid[GetIndex(x, ay)];
				grid[GetIndex(x, ay)] = NULL;
            }
			ay = y;
            i++;
        } /* e_xy+e_y < 0 */
	}
    return (Vector2Int) {Clamp(ax, 0, WIDTH-1), Clamp(ay, 0, HEIGHT-1)};
}

static bool TranslateParticle(particle_t* grid[HEIGHT * WIDTH], int x, int y, int x1, int y1) {
    int memX = x;
    int memY = y;

    bool moved = false;

    int ax = x;
    int ay = y;

	int dx =  abs (x1 - x), sx = x < x1 ? 1 : -1;
	int dy = -abs (y1 - y), sy = y < y1 ? 1 : -1; 
	int err = dx + dy, e2; /* error value e_xy */

	for (int i = 0; i < 5; i++){  /* loop */

		if (x == x1 && y == y1) break;
		e2 = 2 * err;

		if (e2 >= dy) { 
            err += dy;
            x += sx;
			// If not blocked, continue
			if (x < 0 || x >= WIDTH) {
				break;
			}
            else if (GetParticle(grid, x, y) != NULL) {
                if (y + sy >= 0 && y + sy < HEIGHT - 1) {
		            e2 = 2 * err;
					if (e2 <= dx && GetParticle(grid, x, y + sy) == NULL) {
						grid[GetIndex(x, y + sy)] = grid[GetIndex(ax, y)];
						grid[GetIndex(ax, y)] = NULL;

						err += dx;
						y += sy;
						ay = y;
					}
                    else {
                        break;
                    }
                }
                else {
                    break;
                }
            }
            else {
				grid[GetIndex(x, y)] = grid[GetIndex(ax, y)];
				grid[GetIndex(ax, y)] = NULL;
            }
			ax = x;
            moved = true;
        } /* e_xy+e_x > 0 */

	    if (e2 <= dx) {
            err += dx;
            y += sy;
			// If not blocked, continue
			if (y < 0 || y >= HEIGHT ) {
                break;
            }
            else if (grid[GetIndex(x, y)] != NULL) {
                if (x + sx >= 0 && x + sx < WIDTH - 1) {
		            e2 = 2 * err;
					if (e2 >= dy && grid[GetIndex(x + sx, y)] == NULL) {
						grid[GetIndex(x + sx, y)] = grid[GetIndex(x, ay)];
						grid[GetIndex(x, ay)] = NULL;

						err += dy;
						x += sx;
						ax = x;
					}
                    else {
                        break;
                    }
                }
                else {
                    break;
                }
            }
            else {
				grid[GetIndex(x, y)] = grid[GetIndex(x, ay)];
				grid[GetIndex(x, ay)] = NULL;
            }
			ay = y;
			moved = true;

        } /* e_xy+e_y < 0 */
	}
    return moved;
}

static void UpdateSolidStuckParticle(particle_t* grid[HEIGHT * WIDTH], int x, int y) {
    particle_t* p = GetParticle(grid, x, y);

    if (p == NULL || p->hasBeenUpdated) return;

    float dt = GetFrameTime();
    mat_prop_t mat = props[p->mat];

    int randNum = rand();

    if (mat.decaying && randNum % 10 < 4) {
		p->lifeTime -= dt;
        if (p->mat == FIRE) {
            p->color.b = 40 - (1 - p->lifeTime) * 40;
            p->color.g = 140 - (1 - p->lifeTime) * 30;
            p->color.a = 255 - (1 - p->lifeTime) * 150;
        }
		if (p->lifeTime <= 0) {
			grid[GetIndex(x, y)] = NULL;
            if (p->mat == FIRE) {
                grid[GetIndex(x, y)] = CreateParticle(SMOKE);
            }
			free(p);

			return;
		}
    }

    if (mat.acting) {
        if (p->mat == FIRE) {
            // Look for flammable stuff

            int i = GetIndex(x, y);

            if (grid[i + WIDTH] != NULL && props[grid[i + WIDTH]->mat].flammable && randNum % 100 < props[grid[i + WIDTH]->mat].flammableProbability) {
                particle_mat_t someMat = grid[GetIndex(x, y + 1)]->mat;

                if (someMat != WATER) {
                    free(grid[i + WIDTH]);
                    grid[i + WIDTH] = CreateParticle(FIRE);
                }
            }
            else if (grid[i - WIDTH] != NULL && props[grid[i - WIDTH]->mat].flammable && randNum % 100 < props[grid[i - WIDTH]->mat].flammableProbability) {
                int temp = i - WIDTH;
                particle_mat_t someMat = grid[temp]->mat;

                if (someMat == WATER) {
					free(grid[i]);
                    grid[i] = grid[temp];

                    grid[temp] = NULL;
                }
                else {
                    free(grid[temp]);
                    grid[temp] = CreateParticle(FIRE);
                }
            }
            else if (grid[i + 1] != NULL && props[grid[i + 1]->mat].flammable && randNum % 100 < props[grid[i + 1]->mat].flammableProbability) {
                int temp = i + 1;
                particle_mat_t someMat = grid[temp]->mat;

                if (someMat == WATER) {
					free(grid[i]);
                    grid[i] = grid[temp];

                    grid[temp] = NULL;
                }
                else {
                    free(grid[temp]);
                    grid[temp] = CreateParticle(FIRE);
                }
            }
            else if (grid[i - 1] != NULL && props[grid[i - 1]->mat].flammable && randNum % 100 < props[grid[i - 1]->mat].flammableProbability) {
                int temp = i - 1;
                particle_mat_t someMat = grid[temp]->mat;

                if (someMat == WATER) {
					free(grid[i]);
                    grid[i] = grid[temp];

                    grid[temp] = NULL;
                }
                else {
                    free(grid[temp]);
                    grid[temp] = CreateParticle(FIRE);
                }
            }
            else if (randNum % 15 == 0 && grid[i - WIDTH] == NULL) {
                // Emit smoke
                grid[i - WIDTH] = CreateParticle(SMOKE);
            }
        }
    }
}

static void UpdateSolidParticle(particle_t* grid[HEIGHT * WIDTH], int x, int y) {

    particle_t* p = GetParticle(grid, x, y);

    if (p == NULL || p->hasBeenUpdated) return;

    int i = GetIndex(x, y);

    float dt = GetFrameTime();
    mat_prop_t mat = props[p->mat];
    
    if (y < HEIGHT - 1) {
        p->stuck = false;
        int temp = i + WIDTH;
        Vector2Int v = {x, y};

        if (grid[temp] == NULL || props[grid[temp]->mat].type > SOLID) {
            p->velocity.x = Clamp(p->velocity.x * (dt * 5), -mat.maxX, mat.maxX);
            p->velocity.y = Clamp(p->velocity.y + (gravity * dt), -mat.maxY, mat.maxY);
            v = TranslateSolidParticle(grid, x, y, x + p->velocity.x, y + p->velocity.y);
        }
        // Down Right
        else if (x < WIDTH - 1 && (grid[temp + 1] == NULL || props[grid[temp + 1]->mat].type > SOLID)) {
            // Down Left
            if (x > 0 && grid[temp - 1] == NULL || props[grid[temp-1]->mat].type > SOLID) {
                // Boost x velocity
                p->velocity.y *= 0.8; // Makes sure that we don't end up with a bunch of large tips
                p->velocity.x = Clamp(p->velocity.x + (2.0f * dt * (p->velocity.x < 0 ? -1 : 1)), -mat.maxX, mat.maxX);
                v = TranslateSolidParticle(grid, x, y, x + p->velocity.x, y + p->velocity.y);
            }
            else {
                //p->velocity.y *= 0.8;
                p->velocity.x = Clamp(p->velocity.x + (2.0f * dt), 0, mat.maxX);
                v = TranslateSolidParticle(grid, x, y, x + p->velocity.x, y + p->velocity.y);
            }
        }
        // Left
        else if (x > 0 && (grid[temp - 1] == NULL || props[grid[temp-1]->mat].type > SOLID)) {
            //p->velocity.y *= 0.8;
            p->velocity.x = Clamp(p->velocity.x + (-2.0f * dt), -mat.maxX, 0);
            v = TranslateSolidParticle(grid, x, y, x + p->velocity.x, y + p->velocity.y);
        }
        if (v.x != x || v.y != y) {
            p->hasBeenUpdated = true;
        }
    }
}

static void UpdateLiquidParticle(particle_t* grid[HEIGHT * WIDTH], int x, int y) {
    particle_t* p = GetParticle(grid, x, y);

    if (p == NULL || p->hasBeenUpdated) return;

    updatedParticles++;

    float dt = GetFrameTime();
    int randNum = rand();
    int i = GetIndex(x, y);

    mat_prop_t mat = props[p->mat];

    /*
    if (randNum % 10 < 4) {
        switch (p->mat) {
        case WATER:
            p->color.b = 231 + (randNum % 20);
            p->color.g = 115 + (randNum % 10);
            break;
        case LAVA:
            p->color.r = 210 + (randNum % 20);
            p->color.g = 90 + (randNum % 10);
        }
    }
    */

    /*
    if (p->velocity.y < 0) {
        // Upward momentum
        Vector2Int v = {x, y};

        p->velocity.x = Clamp(p->velocity.x + (2 * dt), -mat.maxX, mat.maxX);
        p->velocity.y = Clamp(p->velocity.y + (0.1f * gravity * dt), -mat.maxY, mat.maxY);
        v = TranslateLiquidParticle(grid, x, y, x + p->velocity.x, y + p->velocity.y);
    }
    */
    if (y < HEIGHT) {
        p->stuck = false;
        Vector2Int v = {x, y};

        if (y < HEIGHT - 1) {
            if (CheckValidMove(grid, x, y + 1, LIQUID)) {
				//p->velocity.x = Clamp(p->velocity.x * (dt * 5), -mat.maxX, mat.maxX);
				p->velocity.y = Clamp(p->velocity.y + (gravity * dt), -mat.maxY, mat.maxY);
                p->velocity.x *= 0.8;
                int temp = GetIndex(x, y+1);
			}
			// Down Right
            else if (CheckValidMove(grid, x + 1, y + 1, LIQUID)) {
				// Down Left
                if (CheckValidMove(grid, x - 1, y + 1, LIQUID)) {
					// Boost x velocity, as both directions are valid

					p->velocity.y *= 0.8; // Makes sure that we don't end up with a bunch of large tips
					//p->velocity.x = Clamp(p->velocity.x + (2.0f * dt * (p->velocity.x < 0 ? -1 : 1)), -mat.maxX, mat.maxX);
					//p->velocity.x = 2 * (p->velocity.x < 0 ? -1 : 1);
					p->velocity.x = Clamp(p->velocity.x + (p->velocity.x * dt), -mat.maxX, mat.maxX);
				}
				else {
                    // Go down right
					//p->velocity.x = Clamp(p->velocity.x + (2.0f * dt), 0, mat.maxX);
					p->velocity.y = 2;
					p->velocity.x = 2;
				}
			}
			// Down Left
            else if (CheckValidMove(grid, x - 1, y + 1, LIQUID)) {
				p->velocity.y = 2;
				p->velocity.x = -2;
				//p->velocity.x = Clamp(p->velocity.x + (-2.0f * dt), -mat.maxX, 0);
			}
			else {

				// Check right and left
                if (CheckValidMove(grid, x - 1, y, LIQUID)) {
					p->velocity.y *= 0.8;
					//p->velocity.x = Clamp(p->velocity.x + (2.0f * dt), 0, mat.maxX);
					p->velocity.x = -2;
				}
                else if (CheckValidMove(grid, x + 1, y, LIQUID)) {
                    // Right
					p->velocity.y *= 0.8;
					//p->velocity.x = Clamp(p->velocity.x + (-2.0f * dt), -mat.maxX, 0);
					p->velocity.x = 2;
                }
                else {
					// No where to go.
					p->velocity.x = 0;
                }
			}

		    v = TranslateLiquidParticle(grid, x, y, x + p->velocity.x, y + p->velocity.y);
        }
        else {
            // -- On the floor --
            // Check right and left
            if (CheckValidMove(grid, x + 1, y, LIQUID)) {
				p->velocity.y = 0;
				//p->velocity.x = Clamp(p->velocity.x + (2.0f * dt), 0, mat.maxX);
				p->velocity.x = 2;
            }
            else if (CheckValidMove(grid, x - 1, y, LIQUID)) {
                p->velocity.y = 0;
				//p->velocity.x = Clamp(p->velocity.x + (-2.0f * dt), -mat.maxX, 0);
				p->velocity.x = -2;
            }

		    v = TranslateLiquidParticle(grid, x, y, x + p->velocity.x, y + p->velocity.y);
        }

		if (v.x != x || v.y != y) {
			p->hasBeenUpdated = true;
			x = v.x;
			y = v.y;
		}
    }

    if (mat.acting) {
        if (p->mat == LAVA) {
            // Look for flammable stuff

            if (grid[i + WIDTH] != NULL && props[grid[i + WIDTH]->mat].flammable && randNum % 100 < props[grid[i + WIDTH]->mat].flammableProbability) {
                int temp = i + WIDTH;
                particle_mat_t someMat = grid[temp]->mat;

                if (someMat == WATER) {
                    if (randNum % 2) {
                        free(grid[temp]);
                        grid[temp] = NULL;
                    }
                    else {
                        free(grid[i]);
                        grid[i] = NULL;
                    }
                }
                else {
                    free(grid[temp]);
                    grid[temp] = CreateParticle(FIRE);
                }
            }

            if (grid[i - WIDTH] != NULL && props[grid[i - WIDTH]->mat].flammable && randNum % 100 < props[grid[i - WIDTH]->mat].flammableProbability) {
                int temp = i - WIDTH;
                particle_mat_t someMat = grid[temp]->mat;
                if (someMat == WATER) {
                    if (randNum % 2) {
                        free(grid[temp]);
                        grid[temp] = NULL;
                    }
                    else {
                        free(grid[i]);
                        grid[i] = NULL;
                    }
                }
                else {
                    free(grid[temp]);
                    grid[temp] = CreateParticle(FIRE);
                }
            }
            else if (x < WIDTH - 1 && grid[i + 1] != NULL && props[grid[i + 1]->mat].flammable && randNum % 100 < props[grid[i + 1]->mat].flammableProbability) {
                int temp = i + 1;
                particle_mat_t someMat = grid[temp]->mat;
                if (someMat == WATER) {
                    if (randNum % 2) {
                        free(grid[temp]);
                        grid[temp] = NULL;
                    }
                    else {
                        free(grid[i]);
                        grid[i] = NULL;
                    }
                }
                else {
                    free(grid[temp]);
                    grid[temp] = CreateParticle(FIRE);
                }
            }
            else if (i > 0 && grid[i - 1] != NULL && props[grid[i - 1]->mat].flammable && randNum % 100 < props[grid[i - 1]->mat].flammableProbability) {
                int temp = i - 1;
                particle_mat_t someMat = grid[temp]->mat;
                if (someMat == WATER) {
                    if (randNum % 2) {
                        free(grid[temp]);
                        grid[temp] = NULL;
                    }
                    else {
                        free(grid[i]);
                        grid[i] = NULL;
                    }
                }
                else {
                    free(grid[temp]);
                    grid[temp] = CreateParticle(FIRE);
                }
            }
            if (grid[i - WIDTH] == NULL && randNum % 100 < 2) {
                grid[i - WIDTH] = CreateParticle(SMOKE);
                // Top layer of lava should continue to move around and emit smoke
            }
        }
    }
}

static void UpdateGasParticle(particle_t* grid[HEIGHT * WIDTH], int x, int y) {
    particle_t* p = GetParticle(grid, x, y);

    if (p == NULL || p->hasBeenUpdated) return;

    float dt = GetFrameTime();
    int i = GetIndex(x, y);

    int randNum = rand();
    if (randNum % 10 < 4) {
		p->lifeTime -= dt;
        if (p->lifeTime < 1) {
            p->color.a = 255 - (1 - p->lifeTime) * 150;
        }
		if (p->lifeTime <= 0) {
			grid[i] = NULL;
			free(p);
			return;
		}
    }

    p->velocity.y = Clamp(p->velocity.y + (gravity * dt * -1.5f), -10.0, 10.0);
    int vy = p->velocity.y;
    p->velocity.x = Clamp(p->velocity.x + (0.1f * dt * (randNum % 2 ? 0.5f : -0.5f)), -5.0, 5.0);
    int vx = grid[i]->velocity.x;

    // Straight up
	if (grid[i - WIDTH] == NULL) {
        // Add some variance because it looks kinda cool and seems to solve some issues
	    p->hasBeenUpdated = TranslateParticle(grid, x, y, x + (randNum % 2 ? 1 : -1), y + vy);
	}
    else {
        if (vx > 0) {
            // Particle wants to move to the right
            
            // Up right
			if (x < WIDTH - 1 && grid[i - WIDTH + 1] == NULL) {
				p->hasBeenUpdated = TranslateParticle(grid, x, y, x+vx, y+vy);
			}
            // Right
			else if (x + 1 < WIDTH - 1 && grid[i + 1] == NULL) {
                // Reduce vertical velocity
				p->velocity.y /= 2.0;
				vy = p->velocity.y;

                vx *= 1.5f;
                p->velocity.x = Clamp(p->velocity.x * 1.5f, 0, 5);

				p->hasBeenUpdated = TranslateParticle(grid, x, y, x + vx, y);
			}
            // Up left
			else if (x > 0 && grid[i - WIDTH - 1] == NULL) {
                // Make particle move to the left in the future
                p->velocity.x = -1;
				p->hasBeenUpdated = TranslateParticle(grid, x, y, x-1, y+vy);
			}
            // Left
			else if (x - 1 > 0 && grid[i - 1] == NULL) {
                // Make particle move to the left in the future
                p->velocity.x = randNum % 2 ? -2 : -1;

				p->velocity.y /= 2.0;
				vy = p->velocity.y;

				p->hasBeenUpdated = TranslateParticle(grid, x, y, x + p->velocity.x, y);
			}
            else {
                p->hasBeenUpdated = false;
            }
        }
        else {
            if (vx == 0) {
                vx = -1;
            }
            // Particle wants to move to the left

			if (x > 0 && grid[i - WIDTH - 1] == NULL) {
                // Left up
				p->hasBeenUpdated = TranslateParticle(grid, x, y, x + vx, y+vy);
			}
			else if (x - 1 > 0 && grid[i - 1] == NULL) {
                // Left
				p->velocity.y /= 2.0;
				vy = p->velocity.y;

                vx *= 1.5;
                p->velocity.x = Clamp(p->velocity.x * 1.5, -5, 0);

				p->hasBeenUpdated = TranslateParticle(grid, x, y, x + vx, y);
			}
			else if (x < WIDTH - 1 && grid[i - WIDTH + 1] == NULL) {
                // Right up
                p->velocity.x = 1;
				p->hasBeenUpdated = TranslateParticle(grid, x, y, x+1, y+vy);
			}
			else if (x + 1 < WIDTH - 1 && grid[i + 1] == NULL) {
                // Right
                p->velocity.x = randNum % 2 ? 1 : 2;
				p->velocity.y /= 2.0;
				vy = p->velocity.y;
				p->hasBeenUpdated = TranslateParticle(grid, x, y, x + p->velocity.x, y);
            }
            else {
                p->hasBeenUpdated = false;
            }
        }
    }
}

bool CheckValidMove(particle_t** grid, int x, int y, particle_state_t particleState) {
    if (!withinBounds(x, y)) {
        return false;
    }
    return (grid[GetIndex(x, y)] == NULL || props[grid[GetIndex(x, y)]->mat].type > particleState);
}

static float MinFloat(float a, float b) {
    if (a < b) {
        return a;
    }
    return b;
}

bool withinBounds(int x, int y) {
    return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
}
// Function to check if all surrounding coordinates are taken
float isSurroundedByType(particle_t* grid[WIDTH * HEIGHT], int x, int y, particle_mat_t mat) {
    int dx[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    int dy[] = { -1, -1, -1, 0, 0, 1, 1, 1 };

    float gaming = 0;

    for (int i = 0; i < 8; ++i) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        
        if (withinBounds(nx, ny)) {
            int index = GetIndex(nx, ny);
            if (grid[index] != NULL && grid[index]->mat == mat) {
                gaming += grid[index]->velocity.x;
            }
        }
    }
    return gaming;
}
