#include <stdio.h>
#include "globals.h"
#include <ncurses.h>

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
            mapa_simulacao.grade[i][j].veiculo_id = 0;
            pthread_mutex_init(&mapa_simulacao.grade[i][j].mutex, NULL);

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

#define COR_RUA 1
#define COR_CARRO 2
#define COR_AMB 3
#define COR_CRUZ 4

static char char_direcao(Direcao dir) {
    if (dir == CIMA) return '^';
    if (dir == BAIXO) return 'v';
    if (dir == ESQUERDA) return '<';
    if (dir == DIREITA) return '>';
    return 'O';
}

void imprimir_mapa(int tick, int ativos, int meta) {
    clear(); 
    
    mvprintw(0, 0, "=== SIMULADOR DE TRAFEGO URBANO ===");
    mvprintw(1, 0, "Tick: %d | Veiculos Ativos: %d / %d", tick, ativos, meta);
    mvprintw(2, 0, "Legenda: [=] Rua  [+] Cruzamento  [^v<>] Carros  [A] Ambulancia");

    for (int i = 0; i < LINHAS; i++) {
        for (int j = 0; j < COLUNAS; j++) {
            pthread_mutex_lock(&mapa_simulacao.grade[i][j].mutex);
            int ocupada = mapa_simulacao.grade[i][j].ocupada;
            TipoCelula tipo = mapa_simulacao.grade[i][j].tipo;
            Direcao dir_v = mapa_simulacao.grade[i][j].direcao_veiculo;
            int amb = mapa_simulacao.grade[i][j].eh_ambulancia;
            pthread_mutex_unlock(&mapa_simulacao.grade[i][j].mutex);

            int py = i + 4; 
            int px = j * 3; 

            if (ocupada) {
                if (amb) {
                    attron(COLOR_PAIR(COR_AMB));
                    mvprintw(py, px, " A ");
                    attroff(COLOR_PAIR(COR_AMB));
                } else {
                    attron(COLOR_PAIR(COR_CARRO));
                    mvprintw(py, px, " %c ", char_direcao(dir_v));
                    attroff(COLOR_PAIR(COR_CARRO));
                }
            } else {
                if (tipo == RUA) {
                    attron(COLOR_PAIR(COR_RUA));
                    mvprintw(py, px, " = ");
                    attroff(COLOR_PAIR(COR_RUA));
                } else if (tipo == CRUZAMENTO) {
                    attron(COLOR_PAIR(COR_CRUZ));
                    mvprintw(py, px, " + ");
                    attroff(COLOR_PAIR(COR_CRUZ));
                }
            }
        }
    }
    refresh();
}