#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "veiculo.h"
#include "globals.h"
#include "logger.h"

// Variáveis globais para controle de ciclo de vida dos veículos (mantidas da sua equipe)
int veiculos_ativos = 0;
int ambulancias_ativas = 0;
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

static int sinal_permite_movimento(const Celula *destino, Direcao direcao_movimento) {
    int movimento_horizontal = direcao_movimento == ESQUERDA ||
                               direcao_movimento == DIREITA;
    Cores sinal = movimento_horizontal ? destino->sinal_horizontal :
                                        destino->sinal_vertical;

    return sinal != VERMELHO;
}

static int aguardar_sinal_verde(int origem_i, int origem_j,
                                int destino_i, int destino_j,
                                Direcao direcao_movimento) {
    if (travar_celula(destino_i, destino_j) != 0) {
        return 0;
    }

    Celula *destino = &mapa_simulacao.grade[destino_i][destino_j];
    TipoCelula tipo_origem = mapa_simulacao.grade[origem_i][origem_j].tipo;

    if (!eh_via(destino->tipo)) {
        liberar_celula(destino_i, destino_j);
        return 0;
    }

    if (destino->tipo != CRUZAMENTO || tipo_origem == CRUZAMENTO) {
        liberar_celula(destino_i, destino_j);
        return 1;
    }

    while (simulacao_esta_rodando()) {
        if (sinal_permite_movimento(destino, direcao_movimento)) {
            liberar_celula(destino_i, destino_j);
            return 1;
        }

        pthread_cond_wait(&destino->cond_semaforo, &mutex_celulas[destino_i][destino_j]);
    }

    liberar_celula(destino_i, destino_j);
    return 0;
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

        if (pthread_mutex_trylock(&mutex_celulas[sx][sy]) == 0) {
            if (mapa_simulacao.grade[sx][sy].ocupada == 0) {
                Veiculo* v = (Veiculo*)malloc(sizeof(Veiculo));
                if (!v) {
                    liberar_celula(sx, sy);
                    return -1;
                }

                static pthread_mutex_t mutex_id = PTHREAD_MUTEX_INITIALIZER;
                pthread_mutex_lock(&mutex_id);
                v->id = proximo_id_veiculo++;
                pthread_mutex_unlock(&mutex_id);

                v->x = sx;
                v->y = sy;
                v->direcao_atual = pontos_spawn[idx].direcao_inicial;

                // Chance de 10% de ser ambulância, ou 100% caso não exista nenhuma ativa
                int r_tipo = rand_safe() % 100;

                int num_amb = ambulancias_ativas;

                if (num_amb == 0 || r_tipo < 10) {
                    v->tipo = AMBULANCIA;
                    v->velocidade = RAPIDO;
                    ambulancias_ativas++;
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
                mapa_simulacao.grade[sx][sy].direcao_veiculo = v->direcao_atual;
                mapa_simulacao.grade[sx][sy].eh_ambulancia = (v->tipo == AMBULANCIA);
                mapa_simulacao.grade[sx][sy].tipo_veiculo_ocupante = (v->tipo == AMBULANCIA) ? 1 : 0;
                log_event("Veiculo %d (Tipo: %d) instanciado em [%d, %d]", v->id, v->tipo, sx, sy);

                liberar_celula(sx, sy);

                pthread_t t;
                void* (*funcao_thread)(void*) = (v->tipo == AMBULANCIA) ? thread_ambulancia : thread_veiculo;

                if (pthread_create(&t, NULL, funcao_thread, (void*)v) != 0) {
                    travar_celula(sx, sy);
                    mapa_simulacao.grade[sx][sy].ocupada = 0;
                    mapa_simulacao.grade[sx][sy].veiculo_id = 0;
                    mapa_simulacao.grade[sx][sy].tipo_veiculo_ocupante = 0;
                    liberar_celula(sx, sy);
                    free(v);
                    return -1;
                }

                return 0; // Sucesso
            }
            liberar_celula(sx, sy);
        }
    }
    return -1; // Sem vaga
}

void* thread_veiculo(void* arg) {
    Veiculo* self = (Veiculo*)arg;
    pthread_detach(pthread_self());

    while (1) {
        // === INÍCIO DA ESPERA SÍNCRONA ===
        pthread_mutex_lock(&mutex_relogio);
        int tick_esperado = tick_atual; // Salva o tick atual antes de dormir

        while (tick_atual == tick_esperado && simulacao_rodando) {
            // Dorme consumindo 0% de CPU até o relógio dar o broadcast
            pthread_cond_wait(&cond_relogio, &mutex_relogio);
        }
        int rodando = simulacao_rodando;
        pthread_mutex_unlock(&mutex_relogio);
        // === FIM DA ESPERA SÍNCRONA ===

        if (!rodando) {
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
            travar_celula(self->x, self->y);
            mapa_simulacao.grade[self->x][self->y].ocupada = 0;
            mapa_simulacao.grade[self->x][self->y].veiculo_id = 0;
            mapa_simulacao.grade[self->x][self->y].tipo_veiculo_ocupante = 0;
            liberar_celula(self->x, self->y);

            pthread_mutex_lock(&mutex_veiculos);
            veiculos_ativos--;
            pthread_cond_signal(&cond_spawn);
            pthread_mutex_unlock(&mutex_veiculos);

            log_event("Veiculo %d despawnou no ponto [%d, %d]", self->id, self->x, self->y);
            free(self);
            pthread_exit(NULL);
        }

        // Roteamento
        travar_celula(self->x, self->y);
        Direcao opcoes_direcao = mapa_simulacao.grade[self->x][self->y].direcao;
        liberar_celula(self->x, self->y);

        if (opcoes_direcao == NENHUMA) {
            // Despawn de emergência se preso fora de via
            travar_celula(self->x, self->y);
            mapa_simulacao.grade[self->x][self->y].ocupada = 0;
            mapa_simulacao.grade[self->x][self->y].veiculo_id = 0;
            mapa_simulacao.grade[self->x][self->y].tipo_veiculo_ocupante = 0;
            liberar_celula(self->x, self->y);

            pthread_mutex_lock(&mutex_veiculos);
            veiculos_ativos--;
            pthread_cond_signal(&cond_spawn);
            pthread_mutex_unlock(&mutex_veiculos);

            log_event("Veiculo %d despawnou de emergencia (preso) no ponto [%d, %d]", self->id, self->x, self->y);
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
        Direcao dir_escolhida;
        if (num_opcoes > 1) {
            dir_escolhida = vetor_opcoes[(rand_safe() >> 8) % num_opcoes];
        } else {
            dir_escolhida = vetor_opcoes[0];
        }

        int dest_x = self->x;
        int dest_y = self->y;
        if (dir_escolhida == CIMA) dest_x--;
        else if (dir_escolhida == BAIXO) dest_x++;
        else if (dir_escolhida == ESQUERDA) dest_y--;
        else if (dir_escolhida == DIREITA) dest_y++;

        if (dentro_mapa(dest_x, dest_y)) {
            // 1. O carro tenta passar pelo semáforo usando a regra segura da equipe
            if (!aguardar_sinal_verde(self->x, self->y, dest_x, dest_y, dir_escolhida)) {
                continue;
            }

            // 2. O carro tenta se mover de fato
            int movido = mover_veiculo_celula(self->x, self->y, dest_x, dest_y,
                                              self->id, dir_escolhida);
            // 3. Se o movimento deu certo, atualizamos a interface!
            if (movido == 1) {
                travar_celula(dest_x, dest_y);
                mapa_simulacao.grade[dest_x][dest_y].direcao_veiculo = dir_escolhida;
                mapa_simulacao.grade[dest_x][dest_y].eh_ambulancia = (self->tipo == AMBULANCIA);
                liberar_celula(dest_x, dest_y);

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

    int radar_x[4] = {-1, -1, -1, -1};
    int radar_y[4] = {-1, -1, -1, -1};

    while (1) {
        pthread_mutex_lock(&mutex_relogio);
        int tick_esperado = tick_atual;
        while (tick_atual == tick_esperado && simulacao_rodando) {
            pthread_cond_wait(&cond_relogio, &mutex_relogio);
        }
        int rodando = simulacao_rodando;
        pthread_mutex_unlock(&mutex_relogio);

        if (!rodando) break;

        self->ticks_acumulados++;
        if (self->ticks_acumulados < (int)self->velocidade) continue;
        self->ticks_acumulados = 0;

        if (self->passos_restantes <= 0 && eh_ponto_despawn(self->x, self->y)) {
            log_event("Ambulancia %d despawnou no ponto [%d, %d]", self->id, self->x, self->y);
            break;
        }

        travar_celula(self->x, self->y);
        Direcao opcoes_direcao = mapa_simulacao.grade[self->x][self->y].direcao;
        liberar_celula(self->x, self->y);

        if (opcoes_direcao == NENHUMA) {
            log_event("Ambulancia %d despawnou de emergencia (presa) no ponto [%d, %d]", self->id, self->x, self->y);
            break;
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

        Direcao dir_escolhida;
        if (num_opcoes > 1) {
            dir_escolhida = vetor_opcoes[(rand_safe() >> 8) % num_opcoes];
        } else if (num_opcoes == 1) {
            dir_escolhida = vetor_opcoes[0];
        } else {
            dir_escolhida = self->direcao_atual;
        }

        int dest_x = self->x;
        int dest_y = self->y;
        if (dir_escolhida == CIMA) dest_x--;
        else if (dir_escolhida == BAIXO) dest_x++;
        else if (dir_escolhida == ESQUERDA) dest_y--;
        else if (dir_escolhida == DIREITA) dest_y++;

        int novos_radar_x[4] = {-1, -1, -1, -1};
        int novos_radar_y[4] = {-1, -1, -1, -1};
        int r_x = self->x;
        int r_y = self->y;

        for (int i = 0; i < 4; i++) {
            // Correção: Primeiro processa a célula atual da varredura
            if (dentro_mapa(r_x, r_y) && eh_via(mapa_simulacao.grade[r_x][r_y].tipo)) {
                if (mapa_simulacao.grade[r_x][r_y].tipo == CRUZAMENTO) {
                    novos_radar_x[i] = r_x;
                    novos_radar_y[i] = r_y;
                }
            } else break;

            // Depois avança a projeção para a próxima célula
            if (dir_escolhida == CIMA) r_x--;
            else if (dir_escolhida == BAIXO) r_x++;
            else if (dir_escolhida == ESQUERDA) r_y--;
            else if (dir_escolhida == DIREITA) r_y++;
        }

        for (int i = 0; i < 4; i++) {
            if (novos_radar_x[i] != -1 && novos_radar_y[i] != -1) {
                int ja_estava = 0;
                for (int j = 0; j < 4; j++) {
                    if (radar_x[j] == novos_radar_x[i] && radar_y[j] == novos_radar_y[i]) {
                        ja_estava = 1;
                        radar_x[j] = -1;
                        break;
                    }
                }

                travar_celula(novos_radar_x[i], novos_radar_y[i]);
                Celula* cel = &mapa_simulacao.grade[novos_radar_x[i]][novos_radar_y[i]];
                if (!ja_estava) {
                    cel->override_emergencia++;
                }
                int eh_horizontal = (dir_escolhida == ESQUERDA || dir_escolhida == DIREITA);
                if (eh_horizontal) {
                    cel->sinal_horizontal = VERDE;
                    cel->sinal_vertical = VERMELHO;
                } else {
                    cel->sinal_vertical = VERDE;
                    cel->sinal_horizontal = VERMELHO;
                }
                pthread_cond_broadcast(&cel->cond_semaforo);
                liberar_celula(novos_radar_x[i], novos_radar_y[i]);

                if (!ja_estava) {
                    pthread_mutex_lock(&mutex_veiculos);
                    overrides_ativos++;
                    pthread_mutex_unlock(&mutex_veiculos);
                    log_event("Ambulancia %d ativou OVERRIDE no cruzamento [%d, %d]", self->id, novos_radar_x[i], novos_radar_y[i]);
                }
            }
        }

        for (int j = 0; j < 4; j++) {
            if (radar_x[j] != -1 && radar_y[j] != -1) {
                travar_celula(radar_x[j], radar_y[j]);
                Celula* cel = &mapa_simulacao.grade[radar_x[j]][radar_y[j]];
                if (cel->override_emergencia > 0) cel->override_emergencia--;
                liberar_celula(radar_x[j], radar_y[j]);

                pthread_mutex_lock(&mutex_veiculos);
                overrides_ativos--;
                pthread_mutex_unlock(&mutex_veiculos);
            }
            radar_x[j] = novos_radar_x[j];
            radar_y[j] = novos_radar_y[j];
        }

        if (dentro_mapa(dest_x, dest_y)) {
            int movido = mover_veiculo_celula(self->x, self->y, dest_x, dest_y, self->id, dir_escolhida);

            if (movido) {
                self->x = dest_x;
                self->y = dest_y;
                self->direcao_atual = dir_escolhida;
                if (self->passos_restantes > 0) self->passos_restantes--;
            }
        }
    }

    // Limpa os cruzamentos que estavam no radar
    for (int i = 0; i < 4; i++) {
        if (radar_x[i] != -1 && radar_y[i] != -1 && dentro_mapa(radar_x[i], radar_y[i])) {
            travar_celula(radar_x[i], radar_y[i]);
            if (mapa_simulacao.grade[radar_x[i]][radar_y[i]].override_emergencia > 0) {
                mapa_simulacao.grade[radar_x[i]][radar_y[i]].override_emergencia--;
            }
            liberar_celula(radar_x[i], radar_y[i]);

            pthread_mutex_lock(&mutex_veiculos);
            overrides_ativos--;
            pthread_mutex_unlock(&mutex_veiculos);
        }
    }

    // Remove o veículo da grade
    if (dentro_mapa(self->x, self->y)) {
        travar_celula(self->x, self->y);
        mapa_simulacao.grade[self->x][self->y].ocupada = 0;
        mapa_simulacao.grade[self->x][self->y].veiculo_id = 0;
        mapa_simulacao.grade[self->x][self->y].tipo_veiculo_ocupante = 0;
        liberar_celula(self->x, self->y);
    }

    // Informa a diminuição na contagem
    pthread_mutex_lock(&mutex_veiculos);
    ambulancias_ativas--;
    veiculos_ativos--;
    pthread_cond_signal(&cond_spawn);
    pthread_mutex_unlock(&mutex_veiculos);

    free(self);
    return NULL;
}