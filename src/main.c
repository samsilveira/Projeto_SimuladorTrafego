#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "config.h"
#include "globals.h"
#include "veiculo.h"
#include <ncurses.h>
#include "logger.h"

// === INICIALIZAÇÃO DAS VARIÁVEIS GLOBAIS DO RELÓGIO (Sua Task) ===
pthread_mutex_t mutex_relogio = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_relogio = PTHREAD_COND_INITIALIZER;
int tick_atual = 0;
int overrides_ativos = 0;
int simulacao_rodando = 1;

static int num_veiculos_meta = 0;

int simulacao_esta_rodando(void) {
    pthread_mutex_lock(&mutex_relogio);
    int rodando = simulacao_rodando;
    pthread_mutex_unlock(&mutex_relogio);
    return rodando;
}

static void encerrar_simulacao(void) {
    pthread_mutex_lock(&mutex_relogio);
    simulacao_rodando = 0;
    pthread_cond_broadcast(&cond_relogio);
    pthread_mutex_unlock(&mutex_relogio);

    pthread_mutex_lock(&mutex_veiculos);
    pthread_cond_broadcast(&cond_spawn);
    pthread_mutex_unlock(&mutex_veiculos);

    for (int i = 0; i < LINHAS; i++) {
        for (int j = 0; j < COLUNAS; j++) {
            travar_celula(i, j);
            pthread_cond_broadcast(&mapa_simulacao.grade[i][j].cond_semaforo);
            liberar_celula(i, j);
        }
    }
}

#ifdef _WIN32
static void tratador_sinal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        encerrar_simulacao();
    }
}
#else
static void* thread_sinais(void* arg) {
    sigset_t *sinais = (sigset_t*)arg;
    int sinal_recebido;

    if (sigwait(sinais, &sinal_recebido) == 0) {
        encerrar_simulacao();
    }
    return NULL;
}
#endif

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
    while (simulacao_esta_rodando()) {
        usleep(tick_ms * 1000);

        pthread_mutex_lock(&mutex_relogio);
        int tick = tick_atual;
        pthread_mutex_unlock(&mutex_relogio);

        // Acesso protegido às variáveis globais
        pthread_mutex_lock(&mutex_veiculos);
        int ativos = veiculos_ativos;
        pthread_mutex_unlock(&mutex_veiculos);

        // Agora a ncurses cuida de toda a impressão limpa!
        imprimir_mapa(tick, ativos, num_veiculos_meta);

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
                    travar_celula(i, j);

                    // só influencia nos semáforos inseridos nos CRUZAMENTOS
                    if (mapa_simulacao.grade[i][j].tipo == CRUZAMENTO) {
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

                    }

                    liberar_celula(i, j);
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

    while (simulacao_esta_rodando()) {
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

#ifndef _WIN32
    sigset_t sinais;
#endif

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

#ifdef _WIN32
    signal(SIGINT, tratador_sinal);
    signal(SIGTERM, tratador_sinal);
#else
    sigemptyset(&sinais);
    sigaddset(&sinais, SIGINT);
    sigaddset(&sinais, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &sinais, NULL);
#endif

    // Inicializa o mapa da simulação
    inicializar_mapa();

    // Inicializa o sistema de veículos
    inicializar_sistema_veiculos();
    num_veiculos_meta = cfg.num_veiculos;

    // Semente do gerador aleatório
    srand(time(NULL));

    // === INICIALIZAÇÃO TUI E LOGS ===
    log_init("debug.log");
    log_event("Simulacao iniciada. Meta: %d veiculos", cfg.num_veiculos);

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_CYAN, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);

#ifndef _WIN32
    // === CRIAÇÃO DA THREAD DE SINAIS (Apenas POSIX) ===
    pthread_t thread_sinais_id;
    if (pthread_create(&thread_sinais_id, NULL, thread_sinais, (void*)&sinais) != 0) {
        fprintf(stderr, "Erro ao criar thread de sinais.\n");
        return EXIT_FAILURE;
    }
#endif

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

    // Mantém a main checando o teclado (encerra com 'q' ou Ctrl+C)
    nodelay(stdscr, TRUE);
    while (simulacao_esta_rodando()) {
        int ch = getch();
        if (ch == 'q' || ch == 'Q' || ch == 3) { // 3 é o código ASCII para Ctrl+C
            encerrar_simulacao();
            break;
        }
        usleep(100 * 1000); // Aguarda 100ms
    }

    // Aguarda o encerramento limpo das threads
    pthread_join(thread_relogio_id, NULL);
    pthread_join(thread_spawn_id, NULL);
#ifndef _WIN32
    pthread_join(thread_sinais_id, NULL);
#endif
    destruir_mutexes_mapa();

    endwin();
    log_close();

    return EXIT_SUCCESS;
}
