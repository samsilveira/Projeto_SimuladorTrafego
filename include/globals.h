#ifndef GLOBALS_H
#define GLOBALS_H

#define LINHAS 20
#define COLUNAS 20

typedef enum {
    CIMA, BAIXO, ESQUERDA, DIREITA, NENHUMA
} Direcao;

typedef enum {
    VERDE, AMARELO, VERMELHO
} Cores;

typedef enum {
    RUA, CRUZAMENTO, CALCADA
} TipoCelula;

typedef struct {
    TipoCelula tipo;
    Direcao direcao; // Direção permitida na célula
    int ocupada;     // 0 para livre, 1 para ocupada
} Celula;

typedef struct {
    Celula grade[LINHAS][COLUNAS];
} Mapa;

// Instância global do mapa (a ser definida no .c correspondente)
extern Mapa mapa_simulacao;

void inicializar_mapa();
void imprimir_mapa();

#endif
