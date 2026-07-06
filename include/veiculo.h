#ifndef VEICULO_H
#define VEICULO_H

#include "globals.h"

typedef enum TipoVeiculo {
    CARRO = 0,
    AMBULANCIA = 1
} TipoVeiculo;

typedef enum Velocidade {
    RAPIDO = 1,
    MEDIO = 2,
    LENTO = 4
} Velocidade;

typedef struct Veiculo {
    int id;
    int x; // Linha atual
    int y; // Coluna atual
    Direcao direcao_atual;
    Velocidade velocidade;
    TipoVeiculo tipo;
    int ticks_acumulados; // Contador de ticks desde o último movimento
    int passos_restantes; // Contador de vida / tamanho da rota
} Veiculo;

// Assinaturas das funções da engine de veículos
void* thread_veiculo(void* arg);
int tentar_spawn_veiculo(void);
void inicializar_sistema_veiculos(void);

#endif
