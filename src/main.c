#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TILE_SIZE 40
#define MAP_ROWS 13
#define MAP_COLS 15
#define MAX_BOMBS 5
#define PLAYER_SIZE (TILE_SIZE - 10) // Tamanho do jogador para o cálculo de colisão

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
    Position pos;          
    float timer;
    int range;
    bool active;
    
    // --- NOVO: Controles da Explosão ---
    bool exploding;
    float explosionTimer;
    int expUp, expDown, expLeft, expRight; // Salva o alcance real da explosão em cada lado
} Bomb;

typedef struct Player {
    Position pos;          
    float speed;
    int maxBombs;
    char name[32];         
    Bomb bombs[MAX_BOMBS]; 
} Player;

typedef struct GameMap {
    TileType** grid;       
    int rows;
    int cols;
} GameMap;


// --- FUNÇÕES NOVAS DE LÓGICA E EFEITOS ---

// Checa colisão preditiva, retorna "true" se bater na parede ou em um bloco
bool CheckCollision(GameMap* map, float newX, float newY) {
    // Calcula as bordas do jogador subtraindo um valor mínimo (0.01f) para não travar nas quinas
    int leftTile   = (int)(newX) / TILE_SIZE;
    int rightTile  = (int)(newX + PLAYER_SIZE - 0.01f) / TILE_SIZE;
    int topTile    = (int)(newY) / TILE_SIZE;
    int bottomTile = (int)(newY + PLAYER_SIZE - 0.01f) / TILE_SIZE;

    // Proteção contra bordas do mapa
    if (leftTile < 0 || rightTile >= map->cols || topTile < 0 || bottomTile >= map->rows) return true;

    // Checa as 4 pontas do retângulo do jogador
    if (map->grid[topTile][leftTile] != TILE_EMPTY) return true;
    if (map->grid[topTile][rightTile] != TILE_EMPTY) return true;
    if (map->grid[bottomTile][leftTile] != TILE_EMPTY) return true;
    if (map->grid[bottomTile][rightTile] != TILE_EMPTY) return true;

    return false;
}

// Lida com a destruição do mapa no raio da explosão
void ProcessExplosion(GameMap* map, Bomb* b) {
    int cx = (int)(b->pos.x) / TILE_SIZE;
    int cy = (int)(b->pos.y) / TILE_SIZE;

    b->expRight = 0; b->expLeft = 0; b->expDown = 0; b->expUp = 0;

    // Direita
    for (int r = 1; r <= b->range; r++) {
        int nx = cx + r;
        if (nx >= map->cols || map->grid[cy][nx] == TILE_WALL) break;
        b->expRight++;
        if (map->grid[cy][nx] == TILE_BLOCK) { map->grid[cy][nx] = TILE_EMPTY; break; }
    }
    // Esquerda
    for (int r = 1; r <= b->range; r++) {
        int nx = cx - r;
        if (nx < 0 || map->grid[cy][nx] == TILE_WALL) break;
        b->expLeft++;
        if (map->grid[cy][nx] == TILE_BLOCK) { map->grid[cy][nx] = TILE_EMPTY; break; }
    }
    // Baixo
    for (int r = 1; r <= b->range; r++) {
        int ny = cy + r;
        if (ny >= map->rows || map->grid[ny][cx] == TILE_WALL) break;
        b->expDown++;
        if (map->grid[ny][cx] == TILE_BLOCK) { map->grid[ny][cx] = TILE_EMPTY; break; }
    }
    // Cima
    for (int r = 1; r <= b->range; r++) {
        int ny = cy - r;
        if (ny < 0 || map->grid[ny][cx] == TILE_WALL) break;
        b->expUp++;
        if (map->grid[ny][cx] == TILE_BLOCK) { map->grid[ny][cx] = TILE_EMPTY; break; }
    }
}

// Desenha o ladrilho estilizado da explosão
void DrawExplosionTile(int gridX, int gridY) {
    DrawRectangle(gridX * TILE_SIZE, gridY * TILE_SIZE, TILE_SIZE, TILE_SIZE, ORANGE);
    DrawRectangle(gridX * TILE_SIZE + 5, gridY * TILE_SIZE + 5, TILE_SIZE - 10, TILE_SIZE - 10, YELLOW);
}


// --- FUNÇÕES E ALOCAÇÃO DINÂMICA ---

GameMap* CreateMap(int rows, int cols) {
    GameMap* map = (GameMap*) malloc(sizeof(GameMap));
    map->rows = rows;
    map->cols = cols;
    map->grid = (TileType**) malloc(rows * sizeof(TileType*));
    for (int i = 0; i < rows; i++) {
        map->grid[i] = (TileType*) malloc(cols * sizeof(TileType));
    }

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (r == 0 || r == rows - 1 || c == 0 || c == cols - 1 || (r % 2 == 0 && c % 2 == 0)) {
                map->grid[r][c] = TILE_WALL;
            } else if ((r > 2 || c > 2) && (GetRandomValue(0, 10) > 4)) {
                map->grid[r][c] = TILE_BLOCK;
            } else {
                map->grid[r][c] = TILE_EMPTY;
            }
        }
    }
    return map;
}

Player* CreatePlayer(const char* inputName, float startX, float startY) {
    Player* p = (Player*) malloc(sizeof(Player));
    p->pos.x = startX;
    p->pos.y = startY;
    p->speed = 3.0f;
    p->maxBombs = 2;

    if (strcmp(inputName, "") == 0) strcpy(p->name, "Jogador 1");
    else strcpy(p->name, inputName);

    for (int i = 0; i < MAX_BOMBS; i++) {
        p->bombs[i].active = false;
        p->bombs[i].exploding = false; // Inicializando o estado
        p->bombs[i].range = 2;         // Alcance da bomba
    }
    return p;
}

void PlantBomb(Player* p) {
    for (int i = 0; i < p->maxBombs; i++) {
        if (!p->bombs[i].active) {
            int gridX = (int)(p->pos.x + TILE_SIZE / 2) / TILE_SIZE;
            int gridY = (int)(p->pos.y + TILE_SIZE / 2) / TILE_SIZE;

            p->bombs[i].pos.x = gridX * TILE_SIZE;
            p->bombs[i].pos.y = gridY * TILE_SIZE;
            p->bombs[i].timer = 2.0f; 
            p->bombs[i].active = true;
            p->bombs[i].exploding = false;
            break;
        }
    }
}

void FreeMap(GameMap* map) {
    for (int i = 0; i < map->rows; i++) free(map->grid[i]);
    free(map->grid);
    free(map);
}


// --- FUNÇÃO PRINCIPAL ---
int main(void) {
    const int screenWidth = MAP_COLS * TILE_SIZE;
    const int screenHeight = MAP_ROWS * TILE_SIZE + 40; 

    InitWindow(screenWidth, screenHeight, "Detonatrix - 2D Engine");
    SetTargetFPS(60);

    GameState state = STATE_PLAYING;
    GameMap* map = CreateMap(MAP_ROWS, MAP_COLS);
    Player* player = CreatePlayer("Detonador", TILE_SIZE + 5, TILE_SIZE + 5);

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        // --- LÓGICA DE ATUALIZAÇÃO ---
        if (state == STATE_PLAYING) {
            
            // --- NOVO: Movimentação com Colisão (Eixos separados para deslizar na parede) ---
            float nextX = player->pos.x;
            float nextY = player->pos.y;
            
            if (IsKeyDown(KEY_RIGHT)) nextX += player->speed;
            if (IsKeyDown(KEY_LEFT))  nextX -= player->speed;
            if (!CheckCollision(map, nextX, player->pos.y)) player->pos.x = nextX; // Só move em X se não colidir

            if (IsKeyDown(KEY_DOWN))  nextY += player->speed;
            if (IsKeyDown(KEY_UP))    nextY -= player->speed;
            if (!CheckCollision(map, player->pos.x, nextY)) player->pos.y = nextY; // Só move em Y se não colidir

            // Plantar Bomba
            if (IsKeyPressed(KEY_SPACE)) {
                PlantBomb(player);
            }

            // Atualizar Temporizador das Bombas
            for (int i = 0; i < player->maxBombs; i++) {
                if (player->bombs[i].active) {
                    if (!player->bombs[i].exploding) {
                        player->bombs[i].timer -= deltaTime;
                        // Aciona a explosão
                        if (player->bombs[i].timer <= 0) {
                            player->bombs[i].exploding = true;
                            player->bombs[i].explosionTimer = 0.4f; // Tempo que o efeito fica na tela
                            ProcessExplosion(map, &player->bombs[i]); // Processa blocos destruídos
                        }
                    } else {
                        // Contador para sumir o efeito visual do fogo
                        player->bombs[i].explosionTimer -= deltaTime;
                        if (player->bombs[i].explosionTimer <= 0) {
                            player->bombs[i].active = false;
                            player->bombs[i].exploding = false;
                        }
                    }
                }
            }
        }

        // --- RENDERIZAÇÃO ---
        BeginDrawing();
            ClearBackground(RAYWHITE);

            if (state == STATE_PLAYING) {
                // Desenhar o Mapa
                for (int r = 0; r < map->rows; r++) {
                    for (int c = 0; c < map->cols; c++) {
                        if (map->grid[r][c] == TILE_WALL) {
                            DrawRectangle(c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE - 2, TILE_SIZE - 2, DARKGRAY);
                        } else if (map->grid[r][c] == TILE_BLOCK) {
                            DrawRectangle(c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE - 2, TILE_SIZE - 2, DARKBROWN);
                        } else {
                            DrawRectangle(c * TILE_SIZE, r * TILE_SIZE, TILE_SIZE - 2, TILE_SIZE - 2, LIGHTGRAY);
                        }
                    }
                }

                // Desenhar Bombas e Explosões
                for (int i = 0; i < player->maxBombs; i++) {
                    if (player->bombs[i].active) {
                        if (!player->bombs[i].exploding) {
                            // Bomba não detonada ainda
                            DrawCircle(player->bombs[i].pos.x + TILE_SIZE / 2, player->bombs[i].pos.y + TILE_SIZE / 2, TILE_SIZE / 3, BLACK);
                        } else {
                            // --- NOVO: Desenhar Efeito de Explosão ---
                            int bx = player->bombs[i].pos.x / TILE_SIZE;
                            int by = player->bombs[i].pos.y / TILE_SIZE;
                            
                            DrawExplosionTile(bx, by); // Centro
                            for (int r = 1; r <= player->bombs[i].expRight; r++) DrawExplosionTile(bx + r, by);
                            for (int r = 1; r <= player->bombs[i].expLeft; r++)  DrawExplosionTile(bx - r, by);
                            for (int r = 1; r <= player->bombs[i].expDown; r++)  DrawExplosionTile(bx, by + r);
                            for (int r = 1; r <= player->bombs[i].expUp; r++)    DrawExplosionTile(bx, by - r);
                        }
                    }
                }

                // Desenhar Jogador
                DrawRectangle(player->pos.x, player->pos.y, PLAYER_SIZE, PLAYER_SIZE, BLUE);

                // HUD
                char hudText[64];
                snprintf(hudText, sizeof(hudText), "Jogador: %s", player->name);
                DrawRectangle(0, MAP_ROWS * TILE_SIZE, screenWidth, 40, BLACK);
                DrawText(hudText, 10, MAP_ROWS * TILE_SIZE + 10, 16, WHITE);
            }
        EndDrawing();
    }

    free(player);
    FreeMap(map);
    CloseWindow();

    return 0;
}
