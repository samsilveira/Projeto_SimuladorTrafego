#ifndef GLOBALS_H
#define GLOBALS_H

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
} Celula;

typedef struct Mapa {
    Celula grade[LINHAS][COLUNAS];
} Mapa;

// Instância global do mapa (a ser definida no .c correspondente)
extern Mapa mapa_simulacao;

void inicializar_mapa(void);
void imprimir_mapa(void);

#endif
