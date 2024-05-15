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
    SOLID_STUCK,
    SOLID,
    LIQUID,
    GAS,
} ParticleType;

typedef enum ParticleMaterial {
    NOTHING,
    SAND,
    WATER,
    SMOKE,
    WOOD,
    LAVA,
    STONE,
    FIRE,
} ParticleMaterial;

typedef struct MaterialProperties {
    float modX;
    float modY;
    float maxX;
    float maxY;
    float initLifeTime;
    bool decaying;
    bool acting;
    bool flammable;
    ParticleType type;
    Color initialColor;
} MaterialProperties;

static MaterialProperties props[8] = {
    {0, 0, 0, 0, 0, false, false, false, SOLID_STUCK, {0, 0, 0, 255}}, // Nothing
    {2, 2, 5, 10, 0, false, false, false, SOLID, {255, 203, 0, 255}}, // Sand
    {2, 2, 10, 10, 0, false, false, false, LIQUID, {0, 121, 241, 255}}, // Water
    {2, 2, 5, 10, 5, true, false, false, GAS, {110, 110, 110, 255}}, // Smoke
    {0, 0, 0, 0, 0, false, false, true, SOLID_STUCK, {76, 63, 47, 255}}, // Wood
    {2, 1, 1.5, 4, 0, false, true, false, LIQUID, {255, 101, 32, 255}}, // Lava
    {0, 0, 0, 0, 0, false, false, false, SOLID_STUCK, {140, 140, 140, 255}}, // Stone
    {0, 0, 0, 0, 1, true, true, false, SOLID_STUCK, {255, 180, 10, 255}}, // Fire
};

typedef struct particle {
    unsigned int id;
    float lifeTime;
    Vector2 velocity;
    Color color;
    bool hasBeenUpdated;
    ParticleMaterial mat;
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
int frameCounter = 0;

//----------------------------------------------------------------------------------
// Local Functions Declaration
//----------------------------------------------------------------------------------

static float MinFloat(float a, float b);
static float Clamp(float v, float min, float max);
static Vector2 Vector2Clamp(Vector2 value, Vector2 min, Vector2 max);

static void UpdateSolidParticle(particle* grid[HEIGHT][WIDTH], int x, int y);
static void UpdateLiquidParticle(particle* grid[HEIGHT][WIDTH], int x, int y);
static void UpdateGasParticle(particle* grid[HEIGHT][WIDTH], int x, int y);
static void UpdateSolidStuckParticle(particle* grid[HEIGHT][WIDTH], int x, int y);

static particle* CreateParticle(ParticleMaterial mat);
static void SwapParticles(particle* grid[HEIGHT][WIDTH], int x1, int y1, int x2, int y2);
static bool TranslateParticle(particle* grid[HEIGHT][WIDTH], int x, int y, int x1, int y1);
static void SpawnParticles(particle* grid[HEIGHT][WIDTH], int x, int y, ParticleMaterial material);

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

    // Set initial colors
    props[NOTHING].initialColor = RAYWHITE;
    props[SAND].initialColor = GOLD;
    props[WATER].initialColor = BLUE;
    props[SMOKE].initialColor = DARKGRAY;
    props[WOOD].initialColor = DARKBROWN;
    props[LAVA].initialColor = ORANGE;

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
		if (IsKeyDown(KEY_S)) {
            SpawnParticles(grid, x, y, SAND);
		}
		else if (IsKeyDown(KEY_W)) {
            SpawnParticles(grid, x, y, WATER);
		}
		else if (IsKeyDown(KEY_G)) {
            SpawnParticles(grid, x, y, SMOKE);
		}
		else if (IsKeyDown(KEY_E)) {
            SpawnParticles(grid, x, y, WOOD);
		}
		else if (IsKeyDown(KEY_A)) {
            SpawnParticles(grid, x, y, LAVA);
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
				if (grid[y][x] != NULL && !grid[y][x]->hasBeenUpdated) {
                    switch (props[grid[y][x]->mat].type) {
                    case SOLID:
                        UpdateSolidParticle(grid, x, y);
                        break;
                    case LIQUID:
                        UpdateLiquidParticle(grid, x, y);
                        break;
                    case GAS:
                        UpdateGasParticle(grid, x, y);
                        break;
                    case SOLID_STUCK:
                        UpdateSolidStuckParticle(grid, x, y);
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
                        DrawPixel(x, y, grid[y][x]->color);
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
				if (grid[y][x] != NULL && grid[y][x]->hasBeenUpdated) {
                    grid[y][x]->hasBeenUpdated = false;
				}
			}
		}
    }
#endif

    // De-Initialization
    //--------------------------------------------------------------------------------------
    //
    UnloadRenderTexture(target);

    CloseAudioDevice();     // Close audio context

    CloseWindow();          // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

static void SpawnParticles(particle* grid[HEIGHT][WIDTH], int x, int y, ParticleMaterial mat) {
    if (props[mat].type != SOLID_STUCK) {
		for (int i = -15; i < 16; i++) {
			for (int j = -15; j < 16; j++) {
				if (abs(i) + abs(j) < 20) {
					if (x + i >= 0 && x + i < WIDTH && y + j >= 0 && y + j < HEIGHT && grid[y + j][x + i] == NULL) {
						if ((x + i + j) % 7 == 0 && (y + j) % 5 == 0) {
							grid[y + j][x + i] = CreateParticle(mat);
						}
					}
				}
			}
		}
    }
    else {
		for (int i = -15; i < 16; i++) {
			for (int j = -15; j < 16; j++) {
				if (abs(i) + abs(j) < 20) {
					if (x + i >= 0 && x + i < WIDTH && y + j >= 0 && y + j < HEIGHT && grid[y + j][x + i] == NULL) {
					    grid[y + j][x + i] = CreateParticle(mat);
					}
				}
			}
		}
    }
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

static particle* CreateParticle(ParticleMaterial material) {
    particle* newParticle = (particle*) malloc(sizeof(particle));
    if (newParticle == NULL) {
        perror("Failed to allocate memory for new particle");
        exit(1);
    }
    newParticle->mat = material;
    if (props[material].decaying) {
        newParticle->lifeTime = props[material].initLifeTime;
    }
    newParticle->hasBeenUpdated = false;
    newParticle->color = props[material].initialColor;
    newParticle->velocity.x = 0.0f;
    newParticle->velocity.y = 0.0f;
    return newParticle;
}

static void SwapParticles(particle* grid[HEIGHT][WIDTH], int x1, int y1, int x2, int y2) {
    grid[y2][x2]->hasBeenUpdated = true;

    particle* tmp = grid[y2][x2];
    grid[y2][x2] = grid[y1][x1];
    grid[y1][x1] = tmp;
    tmp->velocity.y = -3;
    tmp->velocity.x = (x1 < x2) ? props[tmp->mat].maxX : -props[tmp->mat].maxX;
}

static bool TranslateParticle(particle* grid[HEIGHT][WIDTH], int x, int y, int x1, int y1) {

    bool moved = false;

    int ax = x;
    int ay = y;

	int dx =  abs (x1 - x), sx = x < x1 ? 1 : -1;
	int dy = -abs (y1 - y), sy = y < y1 ? 1 : -1; 
	int err = dx + dy, e2; /* error value e_xy */

	for (;;){  /* loop */

		if (x == x1 && y == y1) break;
		e2 = 2 * err;

		if (e2 >= dy) { 
            err += dy;
            x += sx;
			// If not blocked, continue
			if (x < 0 || x >= WIDTH  || grid[y][x] != NULL) {
				break;
			}

			grid[y][x] = grid[y][ax];
			grid[y][ax] = NULL;
			ax = x;
            moved = true;
        } /* e_xy+e_x > 0 */

	    if (e2 <= dx) {
            err += dx;
            y += sy;
			// If not blocked, continue
			if (y < 0 || y >= HEIGHT  || grid[y][x] != NULL) {
                break;
			}

			grid[y][x] = grid[ay][x];
			grid[ay][x] = NULL;
			ay = y;
            moved = true;
        } /* e_xy+e_y < 0 */
	}
    return moved;
}

static void UpdateSolidStuckParticle(particle* grid[HEIGHT][WIDTH], int x, int y) {
    particle* p = grid[y][x];

    if (p == NULL || p->hasBeenUpdated) return;

    float dt = GetFrameTime();
    MaterialProperties mat = props[p->mat];

    int randNum = rand();

    if (mat.decaying && randNum % 10 < 4) {
		p->lifeTime -= dt;
		if (p->lifeTime <= 0) {
			grid[y][x] = NULL;
            if (p->mat == FIRE) {
                grid[y][x] = CreateParticle(SMOKE);
            }
			free(p);

			return;
		}
    }

    if (mat.acting) {
        if (p->mat == FIRE) {
            // Look for flammable stuff
            if (grid[y + 1][x] != NULL && props[grid[y + 1][x]->mat].flammable && randNum % 15 == 1) {
                free(grid[y + 1][x]);
                grid[y + 1][x] = CreateParticle(FIRE);
            }
            else if (grid[y - 1][x] != NULL && props[grid[y - 1][x]->mat].flammable && randNum % 15 == 1) {
                free(grid[y - 1][x]);
                grid[y - 1][x] = CreateParticle(FIRE);
            }
            else if (grid[y][x + 1] != NULL && props[grid[y][x + 1]->mat].flammable && randNum % 15 == 1) {
                free(grid[y][x + 1]);
                grid[y][x + 1] = CreateParticle(FIRE);
            }
            else if (grid[y][x - 1] != NULL && props[grid[y][x - 1]->mat].flammable && randNum % 15 == 1) {
                free(grid[y][x-1]);
                grid[y][x-1] = CreateParticle(FIRE);
            }
            else if (randNum % 15 == 0 && grid[y - 1][x] == NULL) {
                // Emit smoke
                grid[y - 1][x] = CreateParticle(SMOKE);
            }
        }
    }
}

static void UpdateSolidParticle(particle* grid[HEIGHT][WIDTH], int x, int y) {

    particle* p = grid[y][x];

    if (p == NULL || p->hasBeenUpdated) return;

    float dt = GetFrameTime();

    p->velocity.y = Clamp(p->velocity.y + (gravity * dt), -10.0, 10.0);
    int vy = p->velocity.y;

    // Down
    if (grid[y + 1][x] == NULL) {
        p->hasBeenUpdated = TranslateParticle(grid, x, y, x, y + vy);
    }
    else if ((props[grid[y + 1][x]->mat].type == GAS || props[grid[y + 1][x]->mat].type == LIQUID) && !grid[y+1][x]->hasBeenUpdated) {
	    p->hasBeenUpdated = true;
        SwapParticles(grid, x, y, x, y + 1);
    }
    // Down left
    else if (grid[y + 1][x-1] == NULL) {
        p->hasBeenUpdated = TranslateParticle(grid, x, y, x - 1, y + vy);
    }
    else if ((props[grid[y + 1][x]->mat].type == GAS || props[grid[y + 1][x]->mat].type == LIQUID) && !grid[y+1][x-1]->hasBeenUpdated) {
	    grid[y][x]->hasBeenUpdated = true;
        SwapParticles(grid, x, y, x - 1, y + 1);
    }
    // Down right
    else if (grid[y + 1][x+1] == NULL) {
        p->hasBeenUpdated = TranslateParticle(grid, x, y, x + 1, y + vy);
    }
    else if ((props[grid[y + 1][x]->mat].type == GAS || props[grid[y + 1][x]->mat].type == LIQUID) && !grid[y+1][x+1]->hasBeenUpdated) {
	    grid[y][x]->hasBeenUpdated = true;
        SwapParticles(grid, x, y, x+1, y + 1);
    }
    else {
        grid[y][x]->velocity.y /= 2.0;
    }
}

static void UpdateLiquidParticle(particle* grid[HEIGHT][WIDTH], int x, int y) {
    particle* p = grid[y][x];

    if (p == NULL || p->hasBeenUpdated) return;

    float dt = GetFrameTime();
    int randNum = rand();

    MaterialProperties matProps = props[p->mat];

    p->velocity.y = Clamp(p->velocity.y + (gravity * dt), -matProps.maxY, matProps.maxY);
    int vy = p->velocity.y;

    p->velocity.x = Clamp(p->velocity.x + (0.01f * dt * (rand() % 2 ? 1 : -1)), -matProps.maxX, matProps.maxX);
    int vx = grid[y][x]->velocity.x;

	if (grid[y+1][x] == NULL) {
        // Add some variance because it looks kinda cool and seems to solve some issues
	    p->hasBeenUpdated = TranslateParticle(grid, x, y, x + (rand() % 2 ? 1 : -1), y + vy);
	}
    else if (props[grid[y + 1][x]->mat].type == GAS) {
        SwapParticles(grid, x, y, x, y + 1);
    }
    else {
        if (vx > 0) {
            // Particle wants to move to the right
            
            // Down right
			if (x < WIDTH - 1 && (grid[y + 1][x + 1] == NULL || props[grid[y + 1][x + 1]->mat].type == GAS)) {
                if (grid[y + 1][x + 1] == NULL) {
				    p->hasBeenUpdated = TranslateParticle(grid, x, y, x+vx, y+vy);
                }
                else {
                    SwapParticles(grid, x, y, x + 1, y + 1);
                }
			}
            // Right
			else if (x + 1 < WIDTH - 1 && grid[y][x + 1] == NULL) {
                // Reduce vertical velocity
				p->velocity.y /= matProps.modY;
				vy = p->velocity.y;

                vx *= matProps.modX;
                p->velocity.x = Clamp(p->velocity.x * matProps.modX, 0, matProps.maxX);

				p->hasBeenUpdated = TranslateParticle(grid, x, y, x + vx, y);
			}
            // Down left
			else if (x > 0 && grid[y + 1][x - 1] == NULL) {
                // Make particle move to the left in the future
                p->velocity.x = -2;
				p->hasBeenUpdated = TranslateParticle(grid, x, y, x-2, y+vy);
			}
            // Left
			else if (x - 1 > 0 && grid[y][x - 1] == NULL) {
                // Make particle move to the left in the future
                p->velocity.x = -2;

				p->velocity.y /= 2.0;
				vy = p->velocity.y;

				p->hasBeenUpdated = TranslateParticle(grid, x, y, x - 2, y);
			}
        }
        else {
            if (vx == 0) {
                vx = -1;
            }
            // Particle wants to move to the left

			if (x - 1 > 0 && (grid[y + 1][x - 1] == NULL || props[grid[y + 1][x - 1]->mat].type == GAS)) {
                // Left down
                if (grid[y + 1][x - 1] == NULL) {
				    p->hasBeenUpdated = TranslateParticle(grid, x, y, x + vx, y+vy);
                }
                else {
                    SwapParticles(grid, x, y, x - 1, y + 1);
                }
			}
			else if (x - 1 > 0 && grid[y][x - 1] == NULL) {
                // Left
				p->velocity.y /= matProps.modY;
				vy = p->velocity.y;

                vx *= matProps.modX;
                p->velocity.x = Clamp(p->velocity.x * matProps.modX, -matProps.maxX, 0);

				p->hasBeenUpdated = TranslateParticle(grid, x, y, x + vx, y);
			}
			else if (x < WIDTH - 1 && grid[y + 1][x + 1] == NULL) {
                // Right down
                p->velocity.x = 2;
				p->hasBeenUpdated = TranslateParticle(grid, x, y, x+2, y+vy);
			}
			else if (x + 1 < WIDTH - 1 && grid[y][x + 1] == NULL) {
                // Right
                p->velocity.x = 2;
				p->velocity.y /= 2.0;
				vy = p->velocity.y;
				p->hasBeenUpdated = TranslateParticle(grid, x, y, x + 2, y);
			}

        }
    }

    if (matProps.acting) {
        if (p->mat == LAVA) {
            // Look for flammable stuff
            if (grid[y + 1][x] != NULL && props[grid[y + 1][x]->mat].flammable && randNum % 2 == 0) {
                free(grid[y + 1][x]);
                grid[y + 1][x] = CreateParticle(FIRE);
            }
            else if (grid[y - 1][x] != NULL && props[grid[y - 1][x]->mat].flammable && randNum % 2 == 1) {
                free(grid[y - 1][x]);
                grid[y - 1][x] = CreateParticle(FIRE);
            }
            else if (x < WIDTH - 1 && grid[y][x + 1] != NULL && props[grid[y][x + 1]->mat].flammable && randNum % 2 == 0) {
                free(grid[y][x + 1]);
                grid[y][x + 1] = CreateParticle(FIRE);
            }
            else if (x > 0 && grid[y][x - 1] != NULL && props[grid[y][x - 1]->mat].flammable && randNum % 2 == 1) {
                free(grid[y][x-1]);
                grid[y][x-1] = CreateParticle(FIRE);
            }
        }
    }
}

static void UpdateGasParticle(particle* grid[HEIGHT][WIDTH], int x, int y) {
    particle* p = grid[y][x];

    if (p == NULL || p->hasBeenUpdated) return;

    float dt = GetFrameTime();

    int randNum = rand();
    if (randNum % 10 < 4) {
		p->lifeTime -= dt;
		if (p->lifeTime <= 0) {
			grid[y][x] = NULL;
			free(p);
			return;
		}
    }

    p->velocity.y = Clamp(p->velocity.y + (gravity * dt * -1.5f), -10.0, 10.0);
    int vy = p->velocity.y;
    p->velocity.x = Clamp(p->velocity.x + (0.1f * dt * (randNum % 2 ? 0.5f : -0.5f)), -5.0, 5.0);
    int vx = grid[y][x]->velocity.x;

    // Straight up
	if (grid[y-1][x] == NULL) {
        // Add some variance because it looks kinda cool and seems to solve some issues
	    p->hasBeenUpdated = TranslateParticle(grid, x, y, x + (randNum % 2 ? 1 : -1), y + vy);
	}
    else {
        if (vx > 0) {
            // Particle wants to move to the right
            
            // Up right
			if (x < WIDTH - 1 && grid[y - 1][x + 1] == NULL) {
				p->hasBeenUpdated = TranslateParticle(grid, x, y, x+vx, y+vy);
			}
            // Right
			else if (x + 1 < WIDTH - 1 && grid[y][x + 1] == NULL) {
                // Reduce vertical velocity
				p->velocity.y /= 2.0;
				vy = p->velocity.y;

                vx *= 1.5f;
                p->velocity.x = Clamp(p->velocity.x * 1.5f, 0, 5);

				p->hasBeenUpdated = TranslateParticle(grid, x, y, x + vx, y);
			}
            // Up left
			else if (x > 0 && grid[y - 1][x - 1] == NULL) {
                // Make particle move to the left in the future
                p->velocity.x = -1;
				p->hasBeenUpdated = TranslateParticle(grid, x, y, x-1, y+vy);
			}
            // Left
			else if (x - 1 > 0 && grid[y][x - 1] == NULL) {
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

			if (x > 0 && grid[y - 1][x - 1] == NULL) {
                // Left up
				p->hasBeenUpdated = TranslateParticle(grid, x, y, x + vx, y+vy);
			}
			else if (x - 1 > 0 && grid[y][x - 1] == NULL) {
                // Left
				p->velocity.y /= 2.0;
				vy = p->velocity.y;

                vx *= 1.5;
                p->velocity.x = Clamp(p->velocity.x * 1.5, -5, 0);

				p->hasBeenUpdated = TranslateParticle(grid, x, y, x + vx, y);
			}
			else if (x < WIDTH - 1 && grid[y - 1][x + 1] == NULL) {
                // Right up
                p->velocity.x = 1;
				p->hasBeenUpdated = TranslateParticle(grid, x, y, x+1, y+vy);
			}
			else if (x + 1 < WIDTH - 1 && grid[y][x + 1] == NULL) {
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
