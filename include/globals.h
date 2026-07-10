#ifndef GLOBALS_H
#define GLOBALS_H

#include <pthread.h>
#include <unistd.h> // Adicionado para as chamadas de tempo do relógio

#define LINHAS 20
#define COLUNAS 20

typedef enum Direcao {
    NENHUMA = 0,
    CIMA = 1 << 0,
    BAIXO = 1 << 1,
    ESQUERDA = 1 << 2,
    DIREITA = 1 << 3
} Direcao;

typedef enum Cores {
    VERDE, AMARELO, VERMELHO
} Cores;

typedef enum TipoCelula {
    RUA, CRUZAMENTO, CALCADA
} TipoCelula;

typedef struct Celula {
    TipoCelula tipo;
    Direcao direcao; // Mascara de direcoes permitidas na celula
    int ocupada;     // 0 para livre, 1 para ocupada
    int veiculo_id;  // ID do veiculo ocupante, ou 0 se livre

    Direcao direcao_veiculo; 
    int eh_ambulancia;
    pthread_mutex_t mutex; // Mutex para exclusão mútua na célula

    // características do semáforo
    Cores sinal_horizontal; // sinal esquerda ou direita
    Cores sinal_vertical; // sinal cima ou baixo
    pthread_cond_t cond_semaforo; // condicao de parada

    // características de emergência (ambulância)
    int override_emergencia; // 1 se ambulância estiver próxima
    int tipo_veiculo_ocupante; // 0 = CARRO, 1 = AMBULANCIA
} Celula;

typedef struct Mapa {
    Celula grade[LINHAS][COLUNAS];
} Mapa;

// Instância global do mapa
extern Mapa mapa_simulacao;
extern pthread_mutex_t mutex_celulas[LINHAS][COLUNAS];

// Variáveis globais para controle de relógio e ticks (Atualizado para sua task)
extern pthread_mutex_t mutex_relogio;
extern pthread_cond_t cond_relogio;
extern int tick_atual;
extern int overrides_ativos; // Indicador global de emergência

// Variáveis globais para controle de ciclo de vida dos veículos
extern int veiculos_ativos;
extern pthread_mutex_t mutex_veiculos;
extern pthread_cond_t cond_spawn;
extern int simulacao_rodando;

void inicializar_mapa(void);

void inicializar_mutexes_mapa(void);
void destruir_mutexes_mapa(void);
int travar_celula(int i, int j);
int liberar_celula(int i, int j);
int mover_veiculo_celula(int origem_i, int origem_j, int destino_i, int destino_j,
                         int veiculo_id, Direcao direcao_movimento);
void imprimir_mapa(int tick, int ativos, int meta);
int simulacao_esta_rodando(void);

#endif
