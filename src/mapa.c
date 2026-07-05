#include <stdio.h>
#include "globals.h"

Mapa mapa_simulacao;

static const Direcao DIRECOES[] = {CIMA, BAIXO, ESQUERDA, DIREITA};

static int dentro_mapa(int linha, int coluna) {
    return linha >= 0 && linha < LINHAS && coluna >= 0 && coluna < COLUNAS;
}

static int eh_via(TipoCelula tipo) {
    return tipo == RUA || tipo == CRUZAMENTO;
}

static int direcao_vertical(Direcao direcao) {
    return (direcao & (CIMA | BAIXO)) != 0;
}

static int direcao_horizontal(Direcao direcao) {
    return (direcao & (ESQUERDA | DIREITA)) != 0;
}

static void marcar_via(int linha, int coluna, Direcao direcao) {
    Celula *celula = &mapa_simulacao.grade[linha][coluna];
    int tinha_vertical = direcao_vertical(celula->direcao);
    int tinha_horizontal = direcao_horizontal(celula->direcao);

    if (celula->tipo == CALCADA) {
        celula->tipo = RUA;
    } else if ((tinha_vertical && direcao_horizontal(direcao)) ||
               (tinha_horizontal && direcao_vertical(direcao))) {
        celula->tipo = CRUZAMENTO;
    }

    celula->direcao = (Direcao)(celula->direcao | direcao);
}

static void adicionar_faixa_horizontal(int linha, int coluna_inicio, int coluna_fim, Direcao direcao) {
    for (int coluna = coluna_inicio; coluna <= coluna_fim; coluna++) {
        marcar_via(linha, coluna, direcao);
    }
}

static void adicionar_faixa_vertical(int coluna, int linha_inicio, int linha_fim, Direcao direcao) {
    for (int linha = linha_inicio; linha <= linha_fim; linha++) {
        marcar_via(linha, coluna, direcao);
    }
}

static int direcao_tem_saida_valida(int linha, int coluna, Direcao direcao) {
    int proxima_linha = linha;
    int proxima_coluna = coluna;

    switch (direcao) {
        case CIMA:     proxima_linha--; break;
        case BAIXO:    proxima_linha++; break;
        case ESQUERDA: proxima_coluna--; break;
        case DIREITA:  proxima_coluna++; break;
        default: return 0;
    }

    return dentro_mapa(proxima_linha, proxima_coluna) &&
           eh_via(mapa_simulacao.grade[proxima_linha][proxima_coluna].tipo);
}

static void remover_direcoes_invalidas(void) {
    for (int linha = 0; linha < LINHAS; linha++) {
        for (int coluna = 0; coluna < COLUNAS; coluna++) {
            Celula *celula = &mapa_simulacao.grade[linha][coluna];

            if (!eh_via(celula->tipo)) {
                continue;
            }

            Direcao direcoes_validas = NENHUMA;

            for (unsigned int i = 0; i < sizeof(DIRECOES) / sizeof(DIRECOES[0]); i++) {
                Direcao direcao = DIRECOES[i];

                if ((celula->direcao & direcao) &&
                    direcao_tem_saida_valida(linha, coluna, direcao)) {
                    direcoes_validas = (Direcao)(direcoes_validas | direcao);
                }
            }

            celula->direcao = direcoes_validas;
        }
    }
}

static char simbolo_rua(Direcao direcao) {
    int mascara = direcao;

    if (mascara == CIMA) {
        return '^';
    }
    if (mascara == BAIXO) {
        return 'v';
    }
    if (mascara == DIREITA) {
        return '>';
    }
    if (mascara == ESQUERDA) {
        return '<';
    }
    if (mascara == (CIMA | BAIXO)) {
        return '|';
    }
    if (mascara == (ESQUERDA | DIREITA)) {
        return '-';
    }
    if (mascara == NENHUMA) {
        return '#';
    }

    return '+';
}

void inicializar_mapa(void) {
    // Inicializa tudo como CALCADA.
    for (int i = 0; i < LINHAS; i++) {
        for (int j = 0; j < COLUNAS; j++) {
            mapa_simulacao.grade[i][j].tipo = CALCADA;
            mapa_simulacao.grade[i][j].direcao = NENHUMA;
            mapa_simulacao.grade[i][j].ocupada = 0;
        }
    }

    // Faixas horizontais em pares: uma para cada sentido da via dupla.
    int ruas_h[] = {2, 3, 6, 7, 10, 11, 14, 15};
    for (int idx = 0; idx < 8; idx++) {
        Direcao direcao = (idx % 2 == 0) ? DIREITA : ESQUERDA;
        adicionar_faixa_horizontal(ruas_h[idx], 1, 18, direcao);
    }

    // Faixas verticais tambem em pares, ligando bordas e miolo do mapa.
    int ruas_v[] = {1, 2, 8, 9, 17, 18};
    for (int idx = 0; idx < 6; idx++) {
        Direcao direcao = (idx % 2 == 0) ? CIMA : BAIXO;
        adicionar_faixa_vertical(ruas_v[idx], 2, 15, direcao);
    }

    // Remove qualquer direcao que terminaria fora da matriz ou em calcada.
    remover_direcoes_invalidas();
}

void imprimir_mapa(void) {
    for (int i = 0; i < LINHAS; i++) {
        for (int j = 0; j < COLUNAS; j++) {
            char c;
            switch (mapa_simulacao.grade[i][j].tipo) {
                case CALCADA: c = '.'; break;
                case RUA: c = simbolo_rua(mapa_simulacao.grade[i][j].direcao); break;
                case CRUZAMENTO: c = 'X'; break;
                default: c = '?'; break;
            }
            printf("%c ", c);
        }
        printf("\n");
    }
}
