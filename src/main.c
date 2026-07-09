#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "config.h"
#include "globals.h"
#include "veiculo.h"

// === INICIALIZAÇÃO DAS VARIÁVEIS GLOBAIS DO RELÓGIO (Sua Task) ===
pthread_mutex_t mutex_relogio = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_relogio = PTHREAD_COND_INITIALIZER;
int tick_atual = 0;
int overrides_ativos = 0;
int simulacao_rodando = 1;

static int num_veiculos_meta = 0;

void print_help(const char *prog_name) {
    printf("Uso: %s -v <veiculos> -t <tick_ms> [-m <mapa.txt>]\n", prog_name);
    printf("Opcoes:\n");
    printf("  -v   Quantidade de carros (obrigatorio)\n");
    printf("  -t   Tempo de delay/tick em ms (obrigatorio)\n");
    printf("  -m   Caminho para o txt de mapa (opcional)\n");
    printf("  -h, --help  Mostra esta mensagem de ajuda\n");
}

// Thread do relógio global discreto.
// Responsável por incrementar o tick global, notificar os veículos e atualizar a exibição.
void* thread_relogio(void* arg) {
    int tick_ms = *(int*)arg;
    while (simulacao_rodando) {
        usleep(tick_ms * 1000);

        // Imprime o estado resultante no terminal (limpando a tela com ANSI escape code)
        printf("\033[H\033[J");
        printf("=== SIMULADOR DE TRAFEGO URBANO ===\n");

        pthread_mutex_lock(&mutex_relogio);
        int tick = tick_atual;
        pthread_mutex_unlock(&mutex_relogio);

        // Esta trava pertence ao sistema de spawn (outra task), mantemos como está
        pthread_mutex_lock(&mutex_veiculos);
        int ativos = veiculos_ativos;
        pthread_mutex_unlock(&mutex_veiculos);

        if (overrides_ativos > 0) {
            printf("Tick: %d | Veiculos Ativos: %d / %d | \033[31m* OVERRIDE DE EMERGENCIA ATIVO *\033[0m\n", tick, ativos, num_veiculos_meta);
        } else {
            printf("Tick: %d | Veiculos Ativos: %d / %d\n", tick, ativos, num_veiculos_meta);
        }

        imprimir_mapa();
        fflush(stdout);

        // Avança o relógio e acorda as threads de veículos
        pthread_mutex_lock(&mutex_relogio);
        tick_atual++;
        int alternar = (tick_atual % 20 == 0);
        pthread_cond_broadcast(&cond_relogio);
        pthread_mutex_unlock(&mutex_relogio);

        // alternância dos relógios
        if (alternar) {
            for (int i = 0; i < LINHAS; i++) {
                for (int j = 0; j < COLUNAS; j++) {
                    // só influencia nos semáforos inseridos nos CRUZAMENTOS
                        if (mapa_simulacao.grade[i][j].tipo == CRUZAMENTO) {
                        pthread_mutex_lock(&mapa_simulacao.grade[i][j].mutex);

                        if (mapa_simulacao.grade[i][j].override_emergencia == 0) {
                            if (mapa_simulacao.grade[i][j].sinal_horizontal == VERDE) {
                                mapa_simulacao.grade[i][j].sinal_horizontal = VERMELHO;
                                mapa_simulacao.grade[i][j].sinal_vertical = VERDE;
                            } else {
                                mapa_simulacao.grade[i][j].sinal_vertical = VERMELHO;
                                mapa_simulacao.grade[i][j].sinal_horizontal = VERDE;
                            }
                            pthread_cond_broadcast(&mapa_simulacao.grade[i][j].cond_semaforo);
                        }

                        pthread_mutex_unlock(&mapa_simulacao.grade[i][j].mutex);
                    }
                }
            }
        }
    }
    return NULL;
}

// Thread gerenciadora de spawn.
// Responsável por manter a quantidade de carros ativa na simulação, acordando quando um carro sai.
void* thread_gerenciadora_spawn(void* arg) {
    (void)arg;

    while (simulacao_rodando) {
        pthread_mutex_lock(&mutex_veiculos);

        while (veiculos_ativos < num_veiculos_meta) {
            if (tentar_spawn_veiculo() == 0) {
                veiculos_ativos++;
            } else {
                // Sem posições de spawn livres no momento, interrompe para tentar novamente mais tarde
                break;
            }
        }

        int precisa_mais = (veiculos_ativos < num_veiculos_meta);

        if (precisa_mais) {
            // Tenta novamente periodicamente enquanto estiver abaixo da meta
            pthread_mutex_unlock(&mutex_veiculos);
            usleep(50 * 1000);
        } else {
            // Só dorme indefinidamente quando já atingiu a meta e precisa aguardar despawns
            pthread_cond_wait(&cond_spawn, &mutex_veiculos);
            pthread_mutex_unlock(&mutex_veiculos);
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    Config cfg = {0};
    int opt;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return EXIT_SUCCESS;
        }
    }

    while ((opt = getopt(argc, argv, "v:t:m:h")) != -1) {
        switch (opt) {
            case 'v': cfg.num_veiculos = atoi(optarg); break;
            case 't': cfg.tick_ms      = atoi(optarg); break;
            case 'm': cfg.mapa_path    = optarg;       break;
            case 'h':
                print_help(argv[0]);
                return EXIT_SUCCESS;
            default:
                print_help(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (cfg.num_veiculos <= 0 || cfg.tick_ms <= 0) {
        fprintf(stderr, "Erro: Argumentos -v e -t sao obrigatorios e devem ser maiores que zero.\n\n");
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }

    // Inicializa o mapa da simulação
    inicializar_mapa();

    // Inicializa o sistema de veículos
    inicializar_sistema_veiculos();
    num_veiculos_meta = cfg.num_veiculos;

    // Semente do gerador aleatório
    srand(time(NULL));

    // Cria a thread gerenciadora de spawn
    pthread_t thread_spawn_id;
    if (pthread_create(&thread_spawn_id, NULL, thread_gerenciadora_spawn, NULL) != 0) {
        fprintf(stderr, "Erro ao criar thread gerenciadora de spawn.\n");
        return EXIT_FAILURE;
    }

    // Cria a thread do relógio global discreto
    pthread_t thread_relogio_id;
    if (pthread_create(&thread_relogio_id, NULL, thread_relogio, (void*)&cfg.tick_ms) != 0) {
        fprintf(stderr, "Erro ao criar thread do relógio discreto.\n");
        return EXIT_FAILURE;
    }

    // Mantém a main rodando até o usuário encerrar ou a simulação parar
    // Para simplificar, a main pode esperar as threads terminarem (o que não acontece na execução contínua)
    pthread_join(thread_relogio_id, NULL);
    pthread_join(thread_spawn_id, NULL);

    return EXIT_SUCCESS;
}