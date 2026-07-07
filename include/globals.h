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
    pthread_mutex_t mutex; // Mutex para exclusão mútua na célula

    // características do semáforo
    Cores sinal_horizontal; // sinal esquerda ou direita
    Cores sinal_vertical; // sinal cima ou baixo
    pthread_cond_t cond_semaforo; // condicao de parada
} Celula;

typedef struct Mapa {
    Celula grade[LINHAS][COLUNAS];
} Mapa;

// Instância global do mapa (a ser definida no .c correspondente)
extern Mapa mapa_simulacao;

// Variáveis globais para controle de relógio e ticks (Atualizado para sua task)
extern pthread_mutex_t mutex_relogio;
extern pthread_cond_t cond_relogio;
extern int tick_atual;

// Variáveis globais para controle de ciclo de vida dos veículos
extern int veiculos_ativos;
extern pthread_mutex_t mutex_veiculos;
extern pthread_cond_t cond_spawn;
extern int simulacao_rodando;

void inicializar_mapa(void);
void imprimir_mapa(void);

#endif