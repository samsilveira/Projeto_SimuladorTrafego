#include <stdio.h>
#include "globals.h"

Mapa mapa_simulacao;

void inicializar_mapa() {
    // Inicializa tudo como CALCADA
    for (int i = 0; i < LINHAS; i++) {
        for (int j = 0; j < COLUNAS; j++) {
            mapa_simulacao.grade[i][j].tipo = CALCADA;
            mapa_simulacao.grade[i][j].direcao = NENHUMA;
            mapa_simulacao.grade[i][j].ocupada = 0;
        }
    }

    // Definindo as ruas horizontais (4 ruas duplas)
    // Direções: DIREITA e ESQUERDA
    int ruas_h[] = {2, 3, 6, 7, 10, 11, 14, 15};
    for (int idx = 0; idx < 8; idx++) {
        int i = ruas_h[idx];
        Direcao d = (idx % 2 == 0) ? DIREITA : ESQUERDA;
        for (int j = 0; j < COLUNAS; j++) {
            mapa_simulacao.grade[i][j].tipo = RUA;
            mapa_simulacao.grade[i][j].direcao = d;
        }
    }

    // Definindo as ruas verticais (2 ruas duplas)
    // Direções: BAIXO e CIMA
    int ruas_v[] = {5, 6, 15, 16};
    for (int idx = 0; idx < 4; idx++) {
        int j = ruas_v[idx];
        Direcao d = (idx % 2 == 0) ? BAIXO : CIMA;
        for (int i = 0; i < LINHAS; i++) {
            // Se já for RUA, vira CRUZAMENTO
            if (mapa_simulacao.grade[i][j].tipo == RUA) {
                mapa_simulacao.grade[i][j].tipo = CRUZAMENTO;
            } else {
                mapa_simulacao.grade[i][j].tipo = RUA;
            }
            // Para simplificação inicial, cruzamentos mantêm a direção da via vertical
            mapa_simulacao.grade[i][j].direcao = d;
        }
    }
}

void imprimir_mapa() {
    for (int i = 0; i < LINHAS; i++) {
        for (int j = 0; j < COLUNAS; j++) {
            char c;
            switch (mapa_simulacao.grade[i][j].tipo) {
                case CALCADA: c = '.'; break;
                case RUA:
                    switch (mapa_simulacao.grade[i][j].direcao) {
                        case CIMA:     c = '^'; break;
                        case BAIXO:    c = 'v'; break;
                        case DIREITA:  c = '>'; break;
                        case ESQUERDA: c = '<'; break;
                        default:       c = '#'; break;
                    }
                    break;
                case CRUZAMENTO: c = 'X'; break;
                default: c = '?'; break;
            }
            printf("%c ", c);
        }
        printf("\n");
    }
}
