#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAMANHO_BLOCO 40
#define LINHAS_MAPA 13
#define COLUNAS_MAPA 15
#define MAX_BOMBAS 5
#define TAMANHO_JOGADOR (TAMANHO_BLOCO - 10)

#define CAMINHO_SPRITE_BOMBA "assets/images/bomb.png" 
#define CAMINHO_SOM_EXPLOSAO "assets/sounds/explosion.wav"
#define CAMINHO_SOM_DERROTA "assets/sounds/lose.wav"

#define CAMINHO_SPRITE_JOGADOR_BAIXO    "assets/images/back-walk.png"
#define CAMINHO_SPRITE_JOGADOR_CIMA     "assets/images/upa-walk.png"
#define CAMINHO_SPRITE_JOGADOR_ESQUERDA "assets/images/left-walk.png"
#define CAMINHO_SPRITE_JOGADOR_DIREITA  "assets/images/right-walk.png"

#define FRAMES_BOMBA 3 
#define FRAMES_JOGADOR 3 

bool pausado = false;

typedef enum EstadoJogo {
    ESTADO_MENU,
    ESTADO_JOGANDO,
    ESTADO_FIM_DE_JOGO
} EstadoJogo;

typedef enum TipoBloco {
    BLOCO_VAZIO,
    BLOCO_PAREDE,
    BLOCO_DESTRUTIVEL
} TipoBloco;

typedef enum Direcao {
    DIRECAO_BAIXO,
    DIRECAO_CIMA,
    DIRECAO_ESQUERDA,
    DIRECAO_DIREITA
} Direcao;

typedef struct Posicao {
    float x;
    float y;
} Posicao;

typedef struct Bomba {
    Posicao pos;
    float temporizador;
    int alcance;
    bool ativa;
    bool explodindo;
    float tempoExplosao;
    int expCima, expBaixo, expEsquerda, expDireita;
    int frameAtual;
    float tempoFrame;
} Bomba;

typedef struct Jogador {
    Posicao pos;
    float velocidade;
    int maxBombas;
    bool vivo;
    char nome[32];
    Bomba bombas[MAX_BOMBAS];
    
    // Variáveis de Animação e Direção
    int frameAtual;
    float tempoFrame;
    bool movendo;
    Direcao direcaoAtual;
} Jogador;

typedef struct MapaJogo {
    TipoBloco** matriz;
    int linhas;
    int colunas;
} MapaJogo;

bool ChecarColisao(MapaJogo* mapa, float novoX, float novoY) {
    int blocoEsquerda = (int)(novoX) / TAMANHO_BLOCO;
    int blocoDireita  = (int)(novoX + TAMANHO_JOGADOR - 0.01f) / TAMANHO_BLOCO;
    int blocoTopo     = (int)(novoY) / TAMANHO_BLOCO;
    int blocoBase     = (int)(novoY + TAMANHO_JOGADOR - 0.01f) / TAMANHO_BLOCO;

    if (blocoEsquerda < 0 || blocoDireita >= mapa->colunas || blocoTopo < 0 || blocoBase >= mapa->linhas) return true;

    if (mapa->matriz[blocoTopo][blocoEsquerda] != BLOCO_VAZIO) return true;
    if (mapa->matriz[blocoTopo][blocoDireita] != BLOCO_VAZIO) return true;
    if (mapa->matriz[blocoBase][blocoEsquerda] != BLOCO_VAZIO) return true;
    if (mapa->matriz[blocoBase][blocoDireita] != BLOCO_VAZIO) return true;

    return false;
}

void ProcessarExplosao(MapaJogo* mapa, Bomba* b) {
    int cx = (int)(b->pos.x) / TAMANHO_BLOCO;
    int cy = (int)(b->pos.y) / TAMANHO_BLOCO;

    b->expDireita = 0; b->expEsquerda = 0; b->expBaixo = 0; b->expCima = 0;

    for (int r = 1; r <= b->alcance; r++) {
        int nx = cx + r;
        if (nx >= mapa->colunas || mapa->matriz[cy][nx] == BLOCO_PAREDE) break;
        b->expDireita++;
        if (mapa->matriz[cy][nx] == BLOCO_DESTRUTIVEL) { break; }
    }
    for (int r = 1; r <= b->alcance; r++) {
        int nx = cx - r;
        if (nx < 0 || mapa->matriz[cy][nx] == BLOCO_PAREDE) break;
        b->expEsquerda++;
        if (mapa->matriz[cy][nx] == BLOCO_DESTRUTIVEL) { break; }
    }
    for (int r = 1; r <= b->alcance; r++) {
        int ny = cy + r;
        if (ny >= mapa->linhas || mapa->matriz[ny][cx] == BLOCO_PAREDE) break;
        b->expBaixo++;
        if (mapa->matriz[ny][cx] == BLOCO_DESTRUTIVEL) { break; }
    }
    for (int r = 1; r <= b->alcance; r++) {
        int ny = cy - r;
        if (ny < 0 || mapa->matriz[ny][cx] == BLOCO_PAREDE) break;
        b->expCima++;
        if (mapa->matriz[ny][cx] == BLOCO_DESTRUTIVEL) { break; }
    }
}

void LimparBlocosDestruidos(MapaJogo* mapa, Bomba* b) {
    int cx = (int)(b->pos.x) / TAMANHO_BLOCO;
    int cy = (int)(b->pos.y) / TAMANHO_BLOCO;

    if (b->expDireita > 0 && mapa->matriz[cy][cx + b->expDireita] == BLOCO_DESTRUTIVEL) 
        mapa->matriz[cy][cx + b->expDireita] = BLOCO_VAZIO;
    if (b->expEsquerda > 0 && mapa->matriz[cy][cx - b->expEsquerda] == BLOCO_DESTRUTIVEL) 
        mapa->matriz[cy][cx - b->expEsquerda] = BLOCO_VAZIO;
    if (b->expBaixo > 0 && mapa->matriz[cy + b->expBaixo][cx] == BLOCO_DESTRUTIVEL) 
        mapa->matriz[cy + b->expBaixo][cx] = BLOCO_VAZIO;
    if (b->expCima > 0 && mapa->matriz[cy - b->expCima][cx] == BLOCO_DESTRUTIVEL) 
        mapa->matriz[cy - b->expCima][cx] = BLOCO_VAZIO;
}

bool ChecarImpactoJogador(Jogador* j, int gridX, int gridY) {
    Rectangle rectJogador = { j->pos.x, j->pos.y, TAMANHO_JOGADOR, TAMANHO_JOGADOR };
    Rectangle rectExplosao = { gridX * TAMANHO_BLOCO, gridY * TAMANHO_BLOCO, TAMANHO_BLOCO, TAMANHO_BLOCO };
    return CheckCollisionRecs(rectJogador, rectExplosao);
}

void DesenharBlocoExplosao(int gridX, int gridY) {
    DrawRectangle(gridX * TAMANHO_BLOCO, gridY * TAMANHO_BLOCO, TAMANHO_BLOCO, TAMANHO_BLOCO, ORANGE);
    DrawRectangle(gridX * TAMANHO_BLOCO + 5, gridY * TAMANHO_BLOCO + 5, TAMANHO_BLOCO - 10, TAMANHO_BLOCO - 10, YELLOW);
}

void DesenharBombaAnimada(Bomba* b, Texture2D textura) {
    if (textura.id == 0) {
        DrawCircle(b->pos.x + TAMANHO_BLOCO / 2, b->pos.y + TAMANHO_BLOCO / 2, TAMANHO_BLOCO / 3, BLACK);
        return;
    }

    float larguraFrame = (float)textura.width / FRAMES_BOMBA;
    float alturaFrame = (float)textura.height;

    Rectangle sourceRec = { b->frameAtual * larguraFrame, 0.0f, larguraFrame, alturaFrame };
    Rectangle destRec = { b->pos.x, b->pos.y, (float)TAMANHO_BLOCO, (float)TAMANHO_BLOCO };
    Vector2 origin = { 0.0f, 0.0f };

    DrawTexturePro(textura, sourceRec, destRec, origin, 0.0f, WHITE);
}

// Desenha o jogador escolhendo a textura correta conforme a direção atual
void DesenharJogador(Jogador* j, Texture2D texBaixo, Texture2D texCima, Texture2D texEsquerda, Texture2D texDireita) {
    Texture2D texturaAtual = texBaixo;

    switch (j->direcaoAtual) {
        case DIRECAO_BAIXO:    texturaAtual = texBaixo; break;
        case DIRECAO_CIMA:     texturaAtual = texCima; break;
        case DIRECAO_ESQUERDA: texturaAtual = texEsquerda; break;
        case DIRECAO_DIREITA:  texturaAtual = texDireita; break;
    }

    if (texturaAtual.id == 0) {
        DrawRectangle(j->pos.x, j->pos.y, TAMANHO_JOGADOR, TAMANHO_JOGADOR, BLUE);
        return;
    }

    float larguraFrame = (float)texturaAtual.width / FRAMES_JOGADOR;
    float alturaFrame = (float)texturaAtual.height;

    Rectangle sourceRec = { j->frameAtual * larguraFrame, 0.0f, larguraFrame, alturaFrame };
    Rectangle destRec = { j->pos.x, j->pos.y, (float)TAMANHO_JOGADOR, (float)TAMANHO_JOGADOR };
    Vector2 origin = { 0.0f, 0.0f };

    DrawTexturePro(texturaAtual, sourceRec, destRec, origin, 0.0f, WHITE);
}

MapaJogo* CriarMapa(int linhas, int colunas) {
    MapaJogo* mapa = (MapaJogo*) malloc(sizeof(MapaJogo));
    mapa->linhas = linhas;
    mapa->colunas = colunas;
    mapa->matriz = (TipoBloco**) malloc(linhas * sizeof(TipoBloco*));
    for (int i = 0; i < linhas; i++) {
        mapa->matriz[i] = (TipoBloco*) malloc(colunas * sizeof(TipoBloco));
    }

    for (int r = 0; r < linhas; r++) {
        for (int c = 0; c < colunas; c++) {
            if (r == 0 || r == linhas - 1 || c == 0 || c == colunas - 1 || (r % 2 == 0 && c % 2 == 0)) {
                mapa->matriz[r][c] = BLOCO_PAREDE;
            } else if ((r > 2 || c > 2) && (GetRandomValue(0, 10) > 4)) {
                mapa->matriz[r][c] = BLOCO_DESTRUTIVEL;
            } else {
                mapa->matriz[r][c] = BLOCO_VAZIO;
            }
        }
    }

    mapa->matriz[1][1] = BLOCO_VAZIO;
    mapa->matriz[1][2] = BLOCO_VAZIO;
    mapa->matriz[2][1] = BLOCO_VAZIO;

    return mapa;
}

Jogador* CriarJogador(const char* nomeEntrada, float startX, float startY) {
    Jogador* j = (Jogador*) malloc(sizeof(Jogador));
    j->pos.x = startX;
    j->pos.y = startY;
    j->velocidade = 3.0f;
    j->maxBombas = 2;
    j->vivo = true;
    j->frameAtual = 0;
    j->tempoFrame = 0.0f;
    j->movendo = false;
    j->direcaoAtual = DIRECAO_BAIXO;

    if (strcmp(nomeEntrada, "") == 0) strcpy(j->nome, "Jogador 1");
    else strcpy(j->nome, nomeEntrada);

    for (int i = 0; i < MAX_BOMBAS; i++) {
        j->bombas[i].ativa = false;
        j->bombas[i].explodindo = false;
        j->bombas[i].alcance = 2;
        j->bombas[i].frameAtual = 0;
        j->bombas[i].tempoFrame = 0.0f;
    }
    return j;
}

void PlantarBomba(Jogador* j) {
    for (int i = 0; i < j->maxBombas; i++) {
        if (!j->bombas[i].ativa) {
            int gridX = (int)(j->pos.x + TAMANHO_JOGADOR / 2) / TAMANHO_BLOCO;
            int gridY = (int)(j->pos.y + TAMANHO_JOGADOR / 2) / TAMANHO_BLOCO;

            j->bombas[i].pos.x = gridX * TAMANHO_BLOCO;
            j->bombas[i].pos.y = gridY * TAMANHO_BLOCO;
            j->bombas[i].temporizador = 2.0f;
            j->bombas[i].ativa = true;
            j->bombas[i].explodindo = false;
            j->bombas[i].frameAtual = 0;
            j->bombas[i].tempoFrame = 0.0f;
            break;
        }
    }
}

void LiberarMapa(MapaJogo* mapa) {
    for (int i = 0; i < mapa->linhas; i++) free(mapa->matriz[i]);
    free(mapa->matriz);
    free(mapa);
}

int main(void) {
    const int larguraTela = COLUNAS_MAPA * TAMANHO_BLOCO;
    const int alturaTela = LINHAS_MAPA * TAMANHO_BLOCO + 40;

    InitWindow(larguraTela, alturaTela, "Detonatrix - Engine 2D");
    InitAudioDevice();
    SetTargetFPS(60);

    // Carregamento de Texturas
    Texture2D texturaBomba = LoadTexture(CAMINHO_SPRITE_BOMBA);
    Texture2D texJogadorBaixo = LoadTexture(CAMINHO_SPRITE_JOGADOR_BAIXO);
    Texture2D texJogadorCima = LoadTexture(CAMINHO_SPRITE_JOGADOR_CIMA);
    Texture2D texJogadorEsquerda = LoadTexture(CAMINHO_SPRITE_JOGADOR_ESQUERDA);
    Texture2D texJogadorDireita = LoadTexture(CAMINHO_SPRITE_JOGADOR_DIREITA);

    // Carregamento de Sons
    Sound somExplosao = LoadSound(CAMINHO_SOM_EXPLOSAO);
    Sound somDerrota = LoadSound(CAMINHO_SOM_DERROTA);

    EstadoJogo estado = ESTADO_JOGANDO;
    MapaJogo* mapa = CriarMapa(LINHAS_MAPA, COLUNAS_MAPA);
    Jogador* jogador = CriarJogador("Detonador", TAMANHO_BLOCO + 5, TAMANHO_BLOCO + 5);

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        if (IsKeyPressed(KEY_Q)) {
            pausado = !pausado;
        }

        if (!pausado && estado == ESTADO_JOGANDO && jogador->vivo) {
            float proximoX = jogador->pos.x;
            float proximoY = jogador->pos.y;
            jogador->movendo = false;

            // Movimentação e ajuste de direção
            if (IsKeyDown(KEY_RIGHT)) { 
                proximoX += jogador->velocidade; 
                jogador->direcaoAtual = DIRECAO_DIREITA;
                jogador->movendo = true;
            }
            else if (IsKeyDown(KEY_LEFT)) { 
                proximoX -= jogador->velocidade; 
                jogador->direcaoAtual = DIRECAO_ESQUERDA;
                jogador->movendo = true;
            }
            if (!ChecarColisao(mapa, proximoX, jogador->pos.y)) jogador->pos.x = proximoX;

            if (IsKeyDown(KEY_UP)) { 
                proximoY -= jogador->velocidade; 
                jogador->direcaoAtual = DIRECAO_CIMA;
                jogador->movendo = true;
            }
            else if (IsKeyDown(KEY_DOWN)) { 
                proximoY += jogador->velocidade; 
                jogador->direcaoAtual = DIRECAO_BAIXO;
                jogador->movendo = true;
            }
            if (!ChecarColisao(mapa, jogador->pos.x, proximoY)) jogador->pos.y = proximoY;

            // Animação de caminhada
            if (jogador->movendo) {
                jogador->tempoFrame += deltaTime;
                if (jogador->tempoFrame >= 0.12f) {
                    jogador->tempoFrame = 0.0f;
                    jogador->frameAtual = (jogador->frameAtual + 1) % FRAMES_JOGADOR;
                }
            } else {
                jogador->frameAtual = 0;
            }

            if (IsKeyPressed(KEY_SPACE)) {
                PlantarBomba(jogador);
            }

            for (int i = 0; i < jogador->maxBombas; i++) {
                if (jogador->bombas[i].ativa) {
                    if (!jogador->bombas[i].explodindo) {
                        jogador->bombas[i].temporizador -= deltaTime;

                        jogador->bombas[i].tempoFrame += deltaTime;
                        if (jogador->bombas[i].tempoFrame >= 0.15f) {
                            jogador->bombas[i].tempoFrame = 0.0f;
                            jogador->bombas[i].frameAtual = (jogador->bombas[i].frameAtual + 1) % FRAMES_BOMBA;
                        }

                        if (jogador->bombas[i].temporizador <= 0) {
                            jogador->bombas[i].explodindo = true;
                            jogador->bombas[i].tempoExplosao = 0.4f;
                            ProcessarExplosao(mapa, &jogador->bombas[i]);
                            PlaySound(somExplosao);
                        }
                    } else {
                        jogador->bombas[i].tempoExplosao -= deltaTime;

                        int bx = jogador->bombas[i].pos.x / TAMANHO_BLOCO;
                        int by = jogador->bombas[i].pos.y / TAMANHO_BLOCO;

                        if (ChecarImpactoJogador(jogador, bx, by)) jogador->vivo = false;
                        for (int r = 1; r <= jogador->bombas[i].expDireita; r++) if (ChecarImpactoJogador(jogador, bx + r, by)) jogador->vivo = false;
                        for (int r = 1; r <= jogador->bombas[i].expEsquerda; r++) if (ChecarImpactoJogador(jogador, bx - r, by)) jogador->vivo = false;
                        for (int r = 1; r <= jogador->bombas[i].expBaixo; r++) if (ChecarImpactoJogador(jogador, bx, by + r)) jogador->vivo = false;
                        for (int r = 1; r <= jogador->bombas[i].expCima; r++) if (ChecarImpactoJogador(jogador, bx, by - r)) jogador->vivo = false;

                        if (jogador->bombas[i].tempoExplosao <= 0) {
                            LimparBlocosDestruidos(mapa, &jogador->bombas[i]);
                            jogador->bombas[i].ativa = false;
                            jogador->bombas[i].explodindo = false;
                        }
                    }
                }
            }

            // Transição para Fim de Jogo e som de derrota
            if (!jogador->vivo) {
                estado = ESTADO_FIM_DE_JOGO;
                PlaySound(somDerrota); // Toca o som de Game Over
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (estado == ESTADO_JOGANDO || estado == ESTADO_FIM_DE_JOGO) {
            for (int r = 0; r < mapa->linhas; r++) {
                for (int c = 0; c < mapa->colunas; c++) {
                    if (mapa->matriz[r][c] == BLOCO_PAREDE) {
                        DrawRectangle(c * TAMANHO_BLOCO, r * TAMANHO_BLOCO, TAMANHO_BLOCO - 2, TAMANHO_BLOCO - 2, DARKGRAY);
                    } else if (mapa->matriz[r][c] == BLOCO_DESTRUTIVEL) {
                        DrawRectangle(c * TAMANHO_BLOCO, r * TAMANHO_BLOCO, TAMANHO_BLOCO - 2, TAMANHO_BLOCO - 2, DARKBROWN);
                    } else {
                        DrawRectangle(c * TAMANHO_BLOCO, r * TAMANHO_BLOCO, TAMANHO_BLOCO - 2, TAMANHO_BLOCO - 2, LIGHTGRAY);
                    }
                }
            }

            for (int i = 0; i < jogador->maxBombas; i++) {
                if (jogador->bombas[i].ativa) {
                    if (!jogador->bombas[i].explodindo) {
                        DesenharBombaAnimada(&jogador->bombas[i], texturaBomba);
                    } else {
                        int bx = jogador->bombas[i].pos.x / TAMANHO_BLOCO;
                        int by = jogador->bombas[i].pos.y / TAMANHO_BLOCO;

                        DesenharBlocoExplosao(bx, by);
                        for (int r = 1; r <= jogador->bombas[i].expDireita; r++) DesenharBlocoExplosao(bx + r, by);
                        for (int r = 1; r <= jogador->bombas[i].expEsquerda; r++) DesenharBlocoExplosao(bx - r, by);
                        for (int r = 1; r <= jogador->bombas[i].expBaixo; r++)    DesenharBlocoExplosao(bx, by + r);
                        for (int r = 1; r <= jogador->bombas[i].expCima; r++)     DesenharBlocoExplosao(bx, by - r);
                    }
                }
            }

            if (jogador->vivo) {
                DesenharJogador(jogador, texJogadorBaixo, texJogadorCima, texJogadorEsquerda, texJogadorDireita);
            }

            char textoHUD[64];
            snprintf(textoHUD, sizeof(textoHUD), "Jogador: %s", jogador->nome);
            DrawRectangle(0, LINHAS_MAPA * TAMANHO_BLOCO, larguraTela, 40, BLACK);
            DrawText(textoHUD, 10, LINHAS_MAPA * TAMANHO_BLOCO + 12, 16, WHITE);
            DrawText("Pressione Q para pausar", larguraTela - 220, LINHAS_MAPA * TAMANHO_BLOCO + 12, 16, WHITE);
        }

        if (estado == ESTADO_FIM_DE_JOGO) {
            DrawRectangle(0, 0, larguraTela, alturaTela, Fade(RED, 0.4f));
            DrawText("GAME OVER", larguraTela / 2 - 110, alturaTela / 2 - 30, 40, WHITE);
        }

        if (pausado && estado == ESTADO_JOGANDO) {
            DrawText("JOGO PAUSADO", larguraTela / 2 - 135, alturaTela / 2 - 35, 35, BLACK);
        }

        EndDrawing();
    }

    // Descarregamento de Recursos
    UnloadTexture(texturaBomba);
    UnloadTexture(texJogadorBaixo);
    UnloadTexture(texJogadorCima);
    UnloadTexture(texJogadorEsquerda);
    UnloadTexture(texJogadorDireita);

    UnloadSound(somExplosao);
    UnloadSound(somDerrota);
    CloseAudioDevice();

    free(jogador);
    LiberarMapa(mapa);
    CloseWindow();

    return 0;
}