#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TILE_SIZE 40
#define MAP_ROWS 13
#define MAP_COLS 15
#define MAX_BOMBS 5

// 1. ENUMERAÇÕES
typedef enum GameState {
    STATE_MENU,
    STATE_PLAYING,
    STATE_GAMEOVER
} GameState;

typedef enum TileType {
    TILE_EMPTY,
    TILE_WALL,
    TILE_BLOCK
} TileType;

// 2. ESTRUTURAS
typedef struct Position {
    float x;
    float y;
} Position;

typedef struct Bomb {
    Position pos;          // Aninhamento de estrutura
    float timer;
    int range;
    bool active;
} Bomb;

typedef struct Player {
    Position pos;          // Aninhamento de estrutura
    float speed;
    int maxBombs;
    char name[32];         // Vetor de char para Strings
    Bomb bombs[MAX_BOMBS]; // Vetor de estruturas
} Player;

typedef struct GameMap {
    TileType** grid;       // Ponteiro para matriz dinâmica (Ponteiro de Ponteiro)
    int rows;
    int cols;
} GameMap;

// 3. FUNÇÕES E ALOCAÇÃO DINÂMICA

// Criação do mapa dinâmico (Matriz + Alocação Dinâmica)
GameMap* CreateMap(int rows, int cols) {
    GameMap* map = (GameMap*) malloc(sizeof(GameMap)); // Alocação dinâmica de estrutura
    map->rows = rows;
    map->cols = cols;

    // Alocação da matriz dinâmica de TileType
    map->grid = (TileType**) malloc(rows * sizeof(TileType*));
    for (int i = 0; i < rows; i++) {
        map->grid[i] = (TileType*) malloc(cols * sizeof(TileType));
    }

    // Preenchimento do labirinto
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (r == 0 || r == rows - 1 || c == 0 || c == cols - 1 || (r % 2 == 0 && c % 2 == 0)) {
                map->grid[r][c] = TILE_WALL;  // Paredes indestrutíveis
            } else if ((r > 2 || c > 2) && (GetRandomValue(0, 10) > 4)) {
                map->grid[r][c] = TILE_BLOCK; // Blocos destrutíveis
            } else {
                map->grid[r][c] = TILE_EMPTY; // Espaço livre
            }
        }
    }
    return map;
}

// Criação do Jogador (Ponteiros de Estruturas + Alocação Dinâmica + Strings)
Player* CreatePlayer(const char* inputName, float startX, float startY) {
    Player* p = (Player*) malloc(sizeof(Player));
    p->pos.x = startX;
    p->pos.y = startY;
    p->speed = 3.0f;
    p->maxBombs = 2;

    // Manipulação de Strings (strcpy, strcmp, strlen)
    if (strcmp(inputName, "") == 0) {
        strcpy(p->name, "Jogador 1");
    } else {
        strcpy(p->name, inputName);
    }

    // Inicialização do vetor de estruturas das bombas
    for (int i = 0; i < MAX_BOMBS; i++) {
        p->bombs[i].active = false;
        p->bombs[i].range = 1;
    }

    return p;
}

// Função para plantio de bombas (Uso de Ponteiros)
void PlantBomb(Player* p) {
    for (int i = 0; i < p->maxBombs; i++) {
        if (!p->bombs[i].active) {
            // Alinha a bomba no centro da célula da grade
            int gridX = (int)(p->pos.x + TILE_SIZE / 2) / TILE_SIZE;
            int gridY = (int)(p->pos.y + TILE_SIZE / 2) / TILE_SIZE;

            p->bombs[i].pos.x = gridX * TILE_SIZE;
            p->bombs[i].pos.y = gridY * TILE_SIZE;
            p->bombs[i].timer = 2.0f; // 2 segundos para explodir
            p->bombs[i].active = true;
            break;
        }
    }
}

// Liberação de Memória Dinâmica
void FreeMap(GameMap* map) {
    for (int i = 0; i < map->rows; i++) {
        free(map->grid[i]); // Libera cada linha da matriz
    }
    free(map->grid);        // Libera os ponteiros das linhas
    free(map);              // Libera a estrutura do mapa
}

// --- FUNÇÃO PRINCIPAL ---
int main(void) {
    const int screenWidth = MAP_COLS * TILE_SIZE;
    const int screenHeight = MAP_ROWS * TILE_SIZE + 40; // Espaço extra para o HUD

    InitWindow(screenWidth, screenHeight, "Detonatrix - 2D Engine");
    SetTargetFPS(60);

    GameState state = STATE_PLAYING;
    GameMap* map = CreateMap(MAP_ROWS, MAP_COLS);
    Player* player = CreatePlayer("Detonador", TILE_SIZE + 5, TILE_SIZE + 5);

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        // --- LÓGICA DE ATUALIZAÇÃO ---
        if (state == STATE_PLAYING) {
            // Movimentação do Jogador
            if (IsKeyDown(KEY_RIGHT)) player->pos.x += player->speed;
            if (IsKeyDown(KEY_LEFT))  player->pos.x -= player->speed;
            if (IsKeyDown(KEY_DOWN))  player->pos.y += player->speed;
            if (IsKeyDown(KEY_UP))    player->pos.y -= player->speed;

            // Plantar Bomba
            if (IsKeyPressed(KEY_SPACE)) {
                PlantBomb(player);
            }

            // Atualizar Temporizador das Bombas
            for (int i = 0; i < player->maxBombs; i++) {
                if (player->bombs[i].active) {
                    player->bombs[i].timer -= deltaTime;
                    if (player->bombs[i].timer <= 0) {
                        player->bombs[i].active = false; // Simulação de explosão
                    }
                }
            }
        }

        // --- RENDERIZAÇÃO ---
        BeginDrawing();
            ClearBackground(RAYWHITE);

            if (state == STATE_PLAYING) {
                // Desenhar o Mapa (Matriz 2D)
                for (int r = 0; r < map->rows; r++) {
                    for (int c = 0; c < map->cols; c++) {
                        if (map->grid[r][c] == TILE_WALL) {
                            DrawRectangle(c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE - 2, TILE_SIZE - 2, DARKGRAY);
                        } else if (map->grid[r][c] == TILE_BLOCK) {
                            DrawRectangle(c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE - 2, TILE_SIZE - 2, BROWN);
                        } else {
                            DrawRectangle(c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE - 2, TILE_SIZE - 2, LIGHTGRAY);
                        }
                    }
                }

                // Desenhar Bombas
                for (int i = 0; i < player->maxBombs; i++) {
                    if (player->bombs[i].active) {
                        DrawCircle(player->bombs[i].pos.x + TILE_SIZE / 2, player->bombs[i].pos.y + TILE_SIZE / 2, TILE_SIZE / 3, RED);
                    }
                }

                // Desenhar Jogador
                DrawRectangle(player->pos.x, player->pos.y, TILE_SIZE - 10, TILE_SIZE - 10, BLUE);

                // HUD / Interface com Manipulação de Strings
                char hudText[64];
                snprintf(hudText, sizeof(hudText), "Jogador: %s | Bombas Ativas: ", player->name);
                DrawRectangle(0, MAP_ROWS * TILE_SIZE, screenWidth, 40, BLACK);
                DrawText(hudText, 10, MAP_ROWS * TILE_SIZE + 10, 16, WHITE);
            }
        EndDrawing();
    }

    // DESALOCAÇÃO DE MEMÓRIA (Obrigatório)
    free(player);
    FreeMap(map);
    CloseWindow();

    return 0;
}