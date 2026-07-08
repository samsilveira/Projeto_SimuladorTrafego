#include <stdio.h>
#include "globals.h"

Mapa mapa_simulacao;
pthread_mutex_t mutex_celulas[LINHAS][COLUNAS];

static const Direcao DIRECOES[] = {CIMA, BAIXO, ESQUERDA, DIREITA};

static int dentro_mapa(int linha, int coluna) {
    return linha >= 0 && linha < LINHAS && coluna >= 0 && coluna < COLUNAS;
}

static int eh_via(TipoCelula tipo) {
    return tipo == RUA || tipo == CRUZAMENTO;
}

void inicializar_mutexes_mapa(void) {
    for (int i = 0; i < LINHAS; i++) {
        for (int j = 0; j < COLUNAS; j++) {
            pthread_mutex_init(&mutex_celulas[i][j], NULL);
        }
    }
}

void destruir_mutexes_mapa(void) {
    for (int i = 0; i < LINHAS; i++) {
        for (int j = 0; j < COLUNAS; j++) {
            pthread_mutex_destroy(&mutex_celulas[i][j]);
        }
    }
}

int travar_celula(int i, int j) {
    if (!dentro_mapa(i, j)) {
        return -1;
    }

    return pthread_mutex_lock(&mutex_celulas[i][j]);
}

int liberar_celula(int i, int j) {
    if (!dentro_mapa(i, j)) {
        return -1;
    }

    return pthread_mutex_unlock(&mutex_celulas[i][j]);
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
    inicializar_mutexes_mapa();

    // Inicializa tudo como CALCADA.
    for (int i = 0; i < LINHAS; i++) {
        for (int j = 0; j < COLUNAS; j++) {
            mapa_simulacao.grade[i][j].tipo = CALCADA;
            mapa_simulacao.grade[i][j].direcao = NENHUMA;
            mapa_simulacao.grade[i][j].ocupada = 0;
            mapa_simulacao.grade[i][j].veiculo_id = 0;

            // inicializa o semáforo (para todas as células)
            pthread_cond_init(&mapa_simulacao.grade[i][j].cond_semaforo, NULL);
            mapa_simulacao.grade[i][j].sinal_horizontal = VERDE;
            mapa_simulacao.grade[i][j].sinal_vertical = VERMELHO;
        }
    }

    // Faixas horizontais em pares: uma para cada sentido da via dupla.
    int ruas_h[] = {2, 3, 6, 7, 10, 11, 14, 15};
    for (int idx = 0; idx < 8; idx++) {
        Direcao direcao = (idx % 2 == 0) ? DIREITA : ESQUERDA;
        adicionar_faixa_horizontal(ruas_h[idx], 1, 18, direcao);
    }

    // Faixas verticais: colunas 1/2 e 17/18 em pares (via dupla).
    // Coluna 8 é via de mão única (CIMA). Coluna 9 não é adicionada (torna-se calçada).
    adicionar_faixa_vertical(1, 2, 15, CIMA);
    adicionar_faixa_vertical(2, 2, 15, BAIXO);
    adicionar_faixa_vertical(8, 2, 15, CIMA); // Mão única
    adicionar_faixa_vertical(17, 2, 15, CIMA);
    adicionar_faixa_vertical(18, 2, 15, BAIXO);

    // Remove qualquer direcao que terminaria fora da matriz ou em calcada.
    remover_direcoes_invalidas();
}

void imprimir_mapa(void) {
    for (int i = 0; i < LINHAS; i++) {
        for (int j = 0; j < COLUNAS; j++) {
            travar_celula(i, j);
            int ocupada = mapa_simulacao.grade[i][j].ocupada;
            int veiculo_id = mapa_simulacao.grade[i][j].veiculo_id;
            TipoCelula tipo = mapa_simulacao.grade[i][j].tipo;
            Direcao direcao = mapa_simulacao.grade[i][j].direcao;
            liberar_celula(i, j);

            if (ocupada) {
                printf("%2d ", veiculo_id);
            } else {
                char c;
                switch (tipo) {
                    case CALCADA: c = '.'; break;
                    case RUA: c = simbolo_rua(direcao); break;
                    case CRUZAMENTO: c = 'X'; break;
                    default: c = '?'; break;
                }
                printf(" %c ", c);
            }
        }
        printf("\n");
    }
}
