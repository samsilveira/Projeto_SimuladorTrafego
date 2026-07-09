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

static int indice_celula(int linha, int coluna) {
    return linha * COLUNAS + coluna;
}

static int sinal_permite_movimento(const Celula *destino, Direcao direcao_movimento) {
    int movimento_horizontal = direcao_movimento == ESQUERDA ||
                               direcao_movimento == DIREITA;
    Cores sinal = movimento_horizontal ? destino->sinal_horizontal :
                                        destino->sinal_vertical;

    return sinal != VERMELHO;
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

int mover_veiculo_celula(int origem_i, int origem_j, int destino_i, int destino_j,
                         int veiculo_id, Direcao direcao_movimento) {
    if (!dentro_mapa(origem_i, origem_j) || !dentro_mapa(destino_i, destino_j)) {
        return -1;
    }

    if (origem_i == destino_i && origem_j == destino_j) {
        return 0;
    }

    int origem_indice = indice_celula(origem_i, origem_j);
    int destino_indice = indice_celula(destino_i, destino_j);
    int primeira_i = origem_i;
    int primeira_j = origem_j;
    int segunda_i = destino_i;
    int segunda_j = destino_j;

    if (destino_indice < origem_indice) {
        primeira_i = destino_i;
        primeira_j = destino_j;
        segunda_i = origem_i;
        segunda_j = origem_j;
    }

    /*
     * Regra de prevencao de deadlock:
     * toda transicao entre duas celulas adquire mutexes pela ordem fixa do
     * indice linear da matriz. Com isso, duas threads nunca formam espera
     * circular tentando trocar de celula ou entrar no mesmo cruzamento.
     */
    if (travar_celula(primeira_i, primeira_j) != 0) {
        return -1;
    }

    if (travar_celula(segunda_i, segunda_j) != 0) {
        liberar_celula(primeira_i, primeira_j);
        return -1;
    }

    Celula *origem = &mapa_simulacao.grade[origem_i][origem_j];
    Celula *destino = &mapa_simulacao.grade[destino_i][destino_j];
    int movido = 0;

    if (eh_via(origem->tipo) &&
        eh_via(destino->tipo) &&
        origem->ocupada &&
        origem->veiculo_id == veiculo_id &&
        destino->ocupada == 0 &&
        (origem->direcao & direcao_movimento) &&
        (origem->tipo == CRUZAMENTO ||
         destino->tipo != CRUZAMENTO ||
         sinal_permite_movimento(destino, direcao_movimento))) {
        
        destino->ocupada = 1;
        destino->veiculo_id = veiculo_id;
        destino->tipo_veiculo_ocupante = origem->tipo_veiculo_ocupante;

        origem->ocupada = 0;
        origem->veiculo_id = 0;
        origem->tipo_veiculo_ocupante = 0;
        
        movido = 1;
    }

    /*
     * A origem so e destravada depois de o destino ja estar travado e a
     * transferencia de ocupacao ter sido decidida.
     */
    liberar_celula(origem_i, origem_j);
    liberar_celula(destino_i, destino_j);

    return movido;
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
            mapa_simulacao.grade[i][j].override_emergencia = 0;
            mapa_simulacao.grade[i][j].tipo_veiculo_ocupante = 0; // 0 para CARRO
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
            int tipo_ocupante = mapa_simulacao.grade[i][j].tipo_veiculo_ocupante;
            int em_override = mapa_simulacao.grade[i][j].override_emergencia;
            liberar_celula(i, j);
            if (ocupada) {
                if (tipo_ocupante == 1) { // 1 = AMBULANCIA
                    printf("\033[31m A \033[0m");
                } else {
                    printf("%2d ", veiculo_id);
                }
            } else {
                char c;
                switch (tipo) {
                    case CALCADA: c = '.'; break;
                    case RUA: c = simbolo_rua(direcao); break;
                    case CRUZAMENTO: c = em_override ? 'E' : 'X'; break;
                    default: c = '?'; break;
                }
                
                if (em_override && tipo == CRUZAMENTO) {
                    printf("\033[31m %c \033[0m", c);
                } else {
                    printf(" %c ", c);
                }
            }
        }
        printf("\n");
    }
}
