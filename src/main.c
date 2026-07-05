#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "config.h"
#include "globals.h"

void print_help(const char *prog_name) {
    printf("Uso: %s -v <veiculos> -t <tick_ms> [-m <mapa.txt>]\n", prog_name);
    printf("Opcoes:\n");
    printf("  -v   Quantidade de carros (obrigatorio)\n");
    printf("  -t   Tempo de delay/tick em ms (obrigatorio)\n");
    printf("  -m   Caminho para o txt de mapa (opcional)\n");
    printf("  -h, --help  Mostra esta mensagem de ajuda\n");
}

int main(int argc, char *argv[]) {
    Config cfg = {0};
    int opt;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return EXIT_SUCCESS;
        }
    }

    while ((opt = getopt(argc, argv, "v:t:m:h")) != -1) {
        switch (opt) {
            case 'v': cfg.num_veiculos = atoi(optarg); break;
            case 't': cfg.tick_ms      = atoi(optarg); break;
            case 'm': cfg.mapa_path    = optarg;       break;
            case 'h':
                print_help(argv[0]);
                return EXIT_SUCCESS;
            default:
                print_help(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (cfg.num_veiculos <= 0 || cfg.tick_ms <= 0) {
        fprintf(stderr, "Erro: Argumentos -v e -t sao obrigatorios e devem ser maiores que zero.\n\n");
        print_help(argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Veiculos: %d | Tick: %dms | Mapa: %s\n",
           cfg.num_veiculos, cfg.tick_ms,
           cfg.mapa_path ? cfg.mapa_path : "(nenhum)");

    inicializar_mapa();
    printf("\nMapa Gerado:\n");
    imprimir_mapa();

    return EXIT_SUCCESS;
}