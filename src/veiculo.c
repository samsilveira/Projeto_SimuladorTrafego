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

                // Chance de 25% de ser ambulância
                int r_tipo = rand_safe() % 100;
                if (r_tipo < 10) {
                    v->tipo = AMBULANCIA;
                    v->velocidade = RAPIDO;
                } else {
                    v->tipo = CARRO;
                    // Distribuição de velocidades
                    int r_vel = rand_safe() % 3;
                    if (r_vel == 0) v->velocidade = RAPIDO;
                    else if (r_vel == 1) v->velocidade = MEDIO;
                    else v->velocidade = LENTO;
                }

                v->ticks_acumulados = 0;
                v->passos_restantes = 25 + rand_safe() % 35; // Entre 25 e 60 passos de vida

                mapa_simulacao.grade[sx][sy].ocupada = 1;
                mapa_simulacao.grade[sx][sy].veiculo_id = v->id;
                mapa_simulacao.grade[sx][sy].tipo_veiculo_ocupante = (v->tipo == AMBULANCIA) ? 1 : 0;

                pthread_mutex_unlock(&mapa_simulacao.grade[sx][sy].mutex);

                pthread_t t;
                void* (*funcao_thread)(void*) = (v->tipo == AMBULANCIA) ? thread_ambulancia : thread_veiculo;

                if (pthread_create(&t, NULL, funcao_thread, (void*)v) != 0) {
                    pthread_mutex_lock(&mapa_simulacao.grade[sx][sy].mutex);
                    mapa_simulacao.grade[sx][sy].ocupada = 0;
                    mapa_simulacao.grade[sx][sy].veiculo_id = 0;
                    mapa_simulacao.grade[sx][sy].tipo_veiculo_ocupante = 0;
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
            mapa_simulacao.grade[self->x][self->y].tipo_veiculo_ocupante = 0;
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
            mapa_simulacao.grade[self->x][self->y].tipo_veiculo_ocupante = 0;
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
            Celula* cel_destino = &mapa_simulacao.grade[dest_x][dest_y];

            if (pthread_mutex_lock(&cel_destino->mutex) != 0) {
                continue;
            }

            // condicao para verificar se o veiculo está entrando no cruzamento ou já está
            int vindo_da_rua = (mapa_simulacao.grade[self->x][self->y].tipo != CRUZAMENTO);

            // se estiver vindo de uma rua para um cruzamento, verifica o semáforo
            // caso já esteja no cruzamento, não precisa verificar o semáforo
            if (cel_destino->tipo == CRUZAMENTO && vindo_da_rua) {

                int eh_horizontal = (dir_escolhida == ESQUERDA || dir_escolhida == DIREITA);

                while (simulacao_rodando) {
                    Cores sinal = eh_horizontal ? cel_destino->sinal_horizontal : cel_destino->sinal_vertical;

                    if (sinal == VERMELHO) {
                        pthread_cond_wait(&cel_destino->cond_semaforo, &cel_destino->mutex);
                    } else {
                        break; // luz verde
                    }
                }
            }

            // luz verde. verifica disponibilidade da célula destino
            if (cel_destino->ocupada == 0) {
                int got_current = 0;
                while (1) {
                    if (pthread_mutex_trylock(&mapa_simulacao.grade[self->x][self->y].mutex) == 0) {
                        got_current = 1;
                        break;
                    }
                    pthread_mutex_unlock(&cel_destino->mutex);
                    usleep(1000); // 1ms backoff
                    if (pthread_mutex_lock(&cel_destino->mutex) != 0) break;
                    if (cel_destino->ocupada != 0) break;
                }

                if (got_current) {
                    mapa_simulacao.grade[self->x][self->y].ocupada = 0;
                    mapa_simulacao.grade[self->x][self->y].veiculo_id = 0;
                    mapa_simulacao.grade[self->x][self->y].tipo_veiculo_ocupante = 0;
                    pthread_mutex_unlock(&mapa_simulacao.grade[self->x][self->y].mutex);

                    cel_destino->ocupada = 1;
                    cel_destino->veiculo_id = self->id;
                    cel_destino->tipo_veiculo_ocupante = 0; // CARRO
                    movido = 1;
                }
            }

            // destranca célula de destino para evitar deadlocks
            pthread_mutex_unlock(&cel_destino->mutex);

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

void* thread_ambulancia(void* arg) {
    Veiculo* self = (Veiculo*)arg;
    pthread_detach(pthread_self());

    int radar_x[3] = {-1, -1, -1};
    int radar_y[3] = {-1, -1, -1};

    while (1) {
        pthread_mutex_lock(&mutex_relogio);
        int tick_esperado = tick_atual;
        while (tick_atual == tick_esperado && simulacao_rodando) {
            pthread_cond_wait(&cond_relogio, &mutex_relogio);
        }
        pthread_mutex_unlock(&mutex_relogio);

        if (!simulacao_rodando) break;

        self->ticks_acumulados++;
        if (self->ticks_acumulados < (int)self->velocidade) continue;
        self->ticks_acumulados = 0;

        if (self->passos_restantes <= 0 && eh_ponto_despawn(self->x, self->y)) {
            for (int i = 0; i < 3; i++) {
                if (radar_x[i] != -1 && radar_y[i] != -1 && dentro_mapa(radar_x[i], radar_y[i])) {
                    pthread_mutex_lock(&mapa_simulacao.grade[radar_x[i]][radar_y[i]].mutex);
                    if (mapa_simulacao.grade[radar_x[i]][radar_y[i]].override_emergencia > 0) {
                        mapa_simulacao.grade[radar_x[i]][radar_y[i]].override_emergencia = 0;
                    }
                    pthread_mutex_unlock(&mapa_simulacao.grade[radar_x[i]][radar_y[i]].mutex);
                    pthread_mutex_lock(&mutex_veiculos);
                    overrides_ativos--;
                    pthread_mutex_unlock(&mutex_veiculos);
                }
            }
            pthread_mutex_lock(&mapa_simulacao.grade[self->x][self->y].mutex);
            mapa_simulacao.grade[self->x][self->y].ocupada = 0;
            mapa_simulacao.grade[self->x][self->y].veiculo_id = 0;
            mapa_simulacao.grade[self->x][self->y].tipo_veiculo_ocupante = 0;
            pthread_mutex_unlock(&mapa_simulacao.grade[self->x][self->y].mutex);
            pthread_mutex_lock(&mutex_veiculos);
            veiculos_ativos--;
            pthread_cond_signal(&cond_spawn);
            pthread_mutex_unlock(&mutex_veiculos);
            free(self);
            pthread_exit(NULL);
        }

        pthread_mutex_lock(&mapa_simulacao.grade[self->x][self->y].mutex);
        Direcao opcoes_direcao = mapa_simulacao.grade[self->x][self->y].direcao;
        pthread_mutex_unlock(&mapa_simulacao.grade[self->x][self->y].mutex);

        if (opcoes_direcao == NENHUMA) {
            for (int i = 0; i < 3; i++) {
                if (radar_x[i] != -1 && radar_y[i] != -1 && dentro_mapa(radar_x[i], radar_y[i])) {
                    pthread_mutex_lock(&mapa_simulacao.grade[radar_x[i]][radar_y[i]].mutex);
                    if (mapa_simulacao.grade[radar_x[i]][radar_y[i]].override_emergencia > 0) {
                        mapa_simulacao.grade[radar_x[i]][radar_y[i]].override_emergencia = 0;
                    }
                    pthread_mutex_unlock(&mapa_simulacao.grade[radar_x[i]][radar_y[i]].mutex);
                    pthread_mutex_lock(&mutex_veiculos);
                    overrides_ativos--;
                    pthread_mutex_unlock(&mutex_veiculos);
                }
            }
            pthread_mutex_lock(&mapa_simulacao.grade[self->x][self->y].mutex);
            mapa_simulacao.grade[self->x][self->y].ocupada = 0;
            mapa_simulacao.grade[self->x][self->y].veiculo_id = 0;
            mapa_simulacao.grade[self->x][self->y].tipo_veiculo_ocupante = 0;
            pthread_mutex_unlock(&mapa_simulacao.grade[self->x][self->y].mutex);
            pthread_mutex_lock(&mutex_veiculos);
            veiculos_ativos--;
            pthread_cond_signal(&cond_spawn);
            pthread_mutex_unlock(&mutex_veiculos);
            free(self);
            pthread_exit(NULL);
        }

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
            if ((opcoes_direcao & d) && d != dir_oposta) vetor_opcoes[num_opcoes++] = d;
        }
        if (num_opcoes == 0 && (opcoes_direcao & dir_oposta)) vetor_opcoes[num_opcoes++] = dir_oposta;
        if (num_opcoes == 0) continue;

        Direcao dir_escolhida = vetor_opcoes[rand_safe() % num_opcoes];

        int dest_x = self->x;
        int dest_y = self->y;
        if (dir_escolhida == CIMA) dest_x--;
        else if (dir_escolhida == BAIXO) dest_x++;
        else if (dir_escolhida == ESQUERDA) dest_y--;
        else if (dir_escolhida == DIREITA) dest_y++;

        int novos_radar_x[3] = {-1, -1, -1};
        int novos_radar_y[3] = {-1, -1, -1};
        int r_x = self->x;
        int r_y = self->y;
        for (int i = 0; i < 3; i++) {
            if (dir_escolhida == CIMA) r_x--;
            else if (dir_escolhida == BAIXO) r_x++;
            else if (dir_escolhida == ESQUERDA) r_y--;
            else if (dir_escolhida == DIREITA) r_y++;

            if (dentro_mapa(r_x, r_y) && eh_via(mapa_simulacao.grade[r_x][r_y].tipo)) {
                if (mapa_simulacao.grade[r_x][r_y].tipo == CRUZAMENTO) {
                    novos_radar_x[i] = r_x;
                    novos_radar_y[i] = r_y;
                }
            } else break;
        }

        for (int i = 0; i < 3; i++) {
            if (novos_radar_x[i] != -1 && novos_radar_y[i] != -1) {
                Celula* cel = &mapa_simulacao.grade[novos_radar_x[i]][novos_radar_y[i]];
                int ja_estava = 0;
                for (int j = 0; j < 3; j++) {
                    if (radar_x[j] == novos_radar_x[i] && radar_y[j] == novos_radar_y[i]) {
                        ja_estava = 1;
                        radar_x[j] = -1;
                        break;
                    }
                }

                pthread_mutex_lock(&cel->mutex);
                cel->override_emergencia = 1;
                int eh_horizontal = (dir_escolhida == ESQUERDA || dir_escolhida == DIREITA);
                if (eh_horizontal) {
                    cel->sinal_horizontal = VERDE;
                    cel->sinal_vertical = VERMELHO;
                } else {
                    cel->sinal_vertical = VERDE;
                    cel->sinal_horizontal = VERMELHO;
                }
                pthread_cond_broadcast(&cel->cond_semaforo);
                pthread_mutex_unlock(&cel->mutex);

                if (!ja_estava) {
                    pthread_mutex_lock(&mutex_veiculos);
                    overrides_ativos++;
                    pthread_mutex_unlock(&mutex_veiculos);
                }
            }
        }

        for (int j = 0; j < 3; j++) {
            if (radar_x[j] != -1 && radar_y[j] != -1) {
                Celula* cel = &mapa_simulacao.grade[radar_x[j]][radar_y[j]];
                pthread_mutex_lock(&cel->mutex);
                if (cel->override_emergencia > 0) cel->override_emergencia = 0;
                pthread_mutex_unlock(&cel->mutex);

                pthread_mutex_lock(&mutex_veiculos);
                overrides_ativos--;
                pthread_mutex_unlock(&mutex_veiculos);
            }
            radar_x[j] = novos_radar_x[j];
            radar_y[j] = novos_radar_y[j];
        }

        if (dentro_mapa(dest_x, dest_y) && eh_via(mapa_simulacao.grade[dest_x][dest_y].tipo)) {
            int movido = 0;
            Celula* cel_destino = &mapa_simulacao.grade[dest_x][dest_y];

            if (pthread_mutex_lock(&cel_destino->mutex) != 0) continue;

            if (cel_destino->ocupada == 0) {
                int got_current = 0;
                while (1) {
                    if (pthread_mutex_trylock(&mapa_simulacao.grade[self->x][self->y].mutex) == 0) {
                        got_current = 1;
                        break;
                    }
                    pthread_mutex_unlock(&cel_destino->mutex);
                    usleep(1000); // 1ms backoff
                    if (pthread_mutex_lock(&cel_destino->mutex) != 0) break;
                    if (cel_destino->ocupada != 0) break;
                }

                if (got_current) {
                    mapa_simulacao.grade[self->x][self->y].ocupada = 0;
                    mapa_simulacao.grade[self->x][self->y].veiculo_id = 0;
                    mapa_simulacao.grade[self->x][self->y].tipo_veiculo_ocupante = 0;
                    pthread_mutex_unlock(&mapa_simulacao.grade[self->x][self->y].mutex);

                    cel_destino->ocupada = 1;
                    cel_destino->veiculo_id = self->id;
                    cel_destino->tipo_veiculo_ocupante = 1;
                    movido = 1;
                }
            }
            pthread_mutex_unlock(&cel_destino->mutex);

            if (movido) {
                self->x = dest_x;
                self->y = dest_y;
                self->direcao_atual = dir_escolhida;
                if (self->passos_restantes > 0) self->passos_restantes--;
            }
        }
    }
    return NULL;
}