#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "veiculo.h"
#include "globals.h"

// Variáveis globais para controle de ciclo de vida dos veículos (mantidas da sua equipe)
int veiculos_ativos = 0;
pthread_mutex_t mutex_veiculos = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_spawn = PTHREAD_COND_INITIALIZER;

// int simulacao_rodando = 1; 

static int proximo_id_veiculo = 1;

static pthread_mutex_t mutex_rand = PTHREAD_MUTEX_INITIALIZER;
static int rand_safe(void) {
    int r;
    pthread_mutex_lock(&mutex_rand);
    r = rand();
    pthread_mutex_unlock(&mutex_rand);
    return r;
}

typedef struct Ponto {
    int linha;
    int coluna;
    Direcao direcao_inicial;
} Ponto;

static Ponto pontos_spawn[] = {
    {2, 1, DIREITA},
    {6, 1, DIREITA},
    {10, 1, DIREITA},
    {14, 1, DIREITA},
    {3, 18, ESQUERDA},
    {7, 18, ESQUERDA},
    {11, 18, ESQUERDA},
    {15, 18, ESQUERDA},
    {15, 1, CIMA},
    {15, 8, CIMA},
    {15, 17, CIMA},
    {2, 2, BAIXO},
    {2, 18, BAIXO}
};
#define NUM_PONTOS_SPAWN (sizeof(pontos_spawn) / sizeof(pontos_spawn[0]))

static int dentro_mapa(int linha, int coluna) {
    return linha >= 0 && linha < LINHAS && coluna >= 0 && coluna < COLUNAS;
}

static int eh_via(TipoCelula tipo) {
    return tipo == RUA || tipo == CRUZAMENTO;
}

static int eh_ponto_despawn(int linha, int coluna) {
    // Retorna verdadeiro se for um ponto periférico de saída de fluxo
    if (coluna == 1 && (linha == 3 || linha == 7 || linha == 11 || linha == 15)) return 1;
    if (coluna == 18 && (linha == 2 || linha == 6 || linha == 10 || linha == 14)) return 1;
    if (linha == 2 && (coluna == 1 || coluna == 8 || coluna == 17)) return 1;
    if (linha == 15 && (coluna == 2 || coluna == 18)) return 1;
    return 0;
}

void inicializar_sistema_veiculos(void) {
    // O sistema é inicializado
    proximo_id_veiculo = 1;
}

int tentar_spawn_veiculo(void) {
    int idx_inicial = rand_safe() % NUM_PONTOS_SPAWN;

    for (int i = 0; i < NUM_PONTOS_SPAWN; i++) {
        int idx = (idx_inicial + i) % NUM_PONTOS_SPAWN;
        int sx = pontos_spawn[idx].linha;
        int sy = pontos_spawn[idx].coluna;

        if (pthread_mutex_trylock(&mapa_simulacao.grade[sx][sy].mutex) == 0) {
            if (mapa_simulacao.grade[sx][sy].ocupada == 0) {
                Veiculo* v = (Veiculo*)malloc(sizeof(Veiculo));
                if (!v) {
                    pthread_mutex_unlock(&mapa_simulacao.grade[sx][sy].mutex);
                    return -1;
                }

                static pthread_mutex_t mutex_id = PTHREAD_MUTEX_INITIALIZER;
                pthread_mutex_lock(&mutex_id);
                v->id = proximo_id_veiculo++;
                pthread_mutex_unlock(&mutex_id);

                v->x = sx;
                v->y = sy;
                v->direcao_atual = pontos_spawn[idx].direcao_inicial;

                // Distribuição de velocidades
                int r_vel = rand_safe() % 3;
                if (r_vel == 0) v->velocidade = RAPIDO;
                else if (r_vel == 1) v->velocidade = MEDIO;
                else v->velocidade = LENTO;

                v->tipo = CARRO;
                v->ticks_acumulados = 0;
                v->passos_restantes = 25 + rand_safe() % 35; // Entre 25 e 60 passos de vida

                mapa_simulacao.grade[sx][sy].ocupada = 1;
                mapa_simulacao.grade[sx][sy].veiculo_id = v->id;

                pthread_mutex_unlock(&mapa_simulacao.grade[sx][sy].mutex);

                pthread_t t;
                if (pthread_create(&t, NULL, thread_veiculo, (void*)v) != 0) {
                    pthread_mutex_lock(&mapa_simulacao.grade[sx][sy].mutex);
                    mapa_simulacao.grade[sx][sy].ocupada = 0;
                    mapa_simulacao.grade[sx][sy].veiculo_id = 0;
                    pthread_mutex_unlock(&mapa_simulacao.grade[sx][sy].mutex);
                    free(v);
                    return -1;
                }

                return 0; // Sucesso
            }
            pthread_mutex_unlock(&mapa_simulacao.grade[sx][sy].mutex);
        }
    }
    return -1; // Sem vaga
}

void* thread_veiculo(void* arg) {
    Veiculo* self = (Veiculo*)arg;
    pthread_detach(pthread_self());

    while (1) {
        // === INÍCIO DA ESPERA SÍNCRONA (Sua Task) ===
        pthread_mutex_lock(&mutex_relogio);
        int tick_esperado = tick_atual; // Salva o tick atual antes de dormir
        
        while (tick_atual == tick_esperado && simulacao_rodando) {
            // Dorme consumindo 0% de CPU até o relógio dar o broadcast
            pthread_cond_wait(&cond_relogio, &mutex_relogio);
        }
        pthread_mutex_unlock(&mutex_relogio);
        // === FIM DA ESPERA SÍNCRONA ===

        if (!simulacao_rodando) {
            break;
        }

        // Controle de velocidade (ignora ticks necessários)
        self->ticks_acumulados++;
        if (self->ticks_acumulados < (int)self->velocidade) {
            continue;
        }
        self->ticks_acumulados = 0;

        // Despawn se atingiu a borda e concluiu o limite de passos
        if (self->passos_restantes <= 0 && eh_ponto_despawn(self->x, self->y)) {
            pthread_mutex_lock(&mapa_simulacao.grade[self->x][self->y].mutex);
            mapa_simulacao.grade[self->x][self->y].ocupada = 0;
            mapa_simulacao.grade[self->x][self->y].veiculo_id = 0;
            pthread_mutex_unlock(&mapa_simulacao.grade[self->x][self->y].mutex);

            pthread_mutex_lock(&mutex_veiculos);
            veiculos_ativos--;
            pthread_cond_signal(&cond_spawn);
            pthread_mutex_unlock(&mutex_veiculos);

            free(self);
            pthread_exit(NULL);
        }

        // Roteamento
        pthread_mutex_lock(&mapa_simulacao.grade[self->x][self->y].mutex);
        Direcao opcoes_direcao = mapa_simulacao.grade[self->x][self->y].direcao;
        pthread_mutex_unlock(&mapa_simulacao.grade[self->x][self->y].mutex);

        if (opcoes_direcao == NENHUMA) {
            // Despawn de emergência se preso fora de via
            pthread_mutex_lock(&mapa_simulacao.grade[self->x][self->y].mutex);
            mapa_simulacao.grade[self->x][self->y].ocupada = 0;
            mapa_simulacao.grade[self->x][self->y].veiculo_id = 0;
            pthread_mutex_unlock(&mapa_simulacao.grade[self->x][self->y].mutex);

            pthread_mutex_lock(&mutex_veiculos);
            veiculos_ativos--;
            pthread_cond_signal(&cond_spawn);
            pthread_mutex_unlock(&mutex_veiculos);

            free(self);
            pthread_exit(NULL);
        }

        // Filtra a direção oposta para evitar fazer curva de 180 graus
        Direcao dir_oposta = NENHUMA;
        if (self->direcao_atual == CIMA) dir_oposta = BAIXO;
        else if (self->direcao_atual == BAIXO) dir_oposta = CIMA;
        else if (self->direcao_atual == ESQUERDA) dir_oposta = DIREITA;
        else if (self->direcao_atual == DIREITA) dir_oposta = ESQUERDA;

        Direcao vetor_opcoes[4];
        int num_opcoes = 0;
        Direcao direcoes_padrao[] = {CIMA, BAIXO, ESQUERDA, DIREITA};

        for (int i = 0; i < 4; i++) {
            Direcao d = direcoes_padrao[i];
            if ((opcoes_direcao & d) && d != dir_oposta) {
                vetor_opcoes[num_opcoes++] = d;
            }
        }

        // Se a única direção viável é a contramão de volta (rua sem saída), aceitamos.
        if (num_opcoes == 0 && (opcoes_direcao & dir_oposta)) {
            vetor_opcoes[num_opcoes++] = dir_oposta;
        }

        if (num_opcoes == 0) {
            continue;
        }

        // Sorteia direção no cruzamento/via
        Direcao dir_escolhida = vetor_opcoes[rand_safe() % num_opcoes];

        int dest_x = self->x;
        int dest_y = self->y;
        if (dir_escolhida == CIMA) dest_x--;
        else if (dir_escolhida == BAIXO) dest_x++;
        else if (dir_escolhida == ESQUERDA) dest_y--;
        else if (dir_escolhida == DIREITA) dest_y++;

        if (dentro_mapa(dest_x, dest_y) && eh_via(mapa_simulacao.grade[dest_x][dest_y].tipo)) {
            // Movimentação com segurança contra deadlocks
            int movido = 0;
            if (pthread_mutex_trylock(&mapa_simulacao.grade[dest_x][dest_y].mutex) == 0) {
                if (mapa_simulacao.grade[dest_x][dest_y].ocupada == 0) {
                    if (pthread_mutex_trylock(&mapa_simulacao.grade[self->x][self->y].mutex) == 0) {
                        mapa_simulacao.grade[self->x][self->y].ocupada = 0;
                        mapa_simulacao.grade[self->x][self->y].veiculo_id = 0;
                        pthread_mutex_unlock(&mapa_simulacao.grade[self->x][self->y].mutex);

                        mapa_simulacao.grade[dest_x][dest_y].ocupada = 1;
                        mapa_simulacao.grade[dest_x][dest_y].veiculo_id = self->id;
                        movido = 1;
                    }
                }
                pthread_mutex_unlock(&mapa_simulacao.grade[dest_x][dest_y].mutex);
            }

            if (movido) {
                self->x = dest_x;
                self->y = dest_y;
                self->direcao_atual = dir_escolhida;
                if (self->passos_restantes > 0) {
                    self->passos_restantes--;
                }
            }
        }
    }

    return NULL;
}