#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "config.h"
#include "globals.h"

int main(int argc, char *argv[]) {
    Config cfg = {0};
    int opt;

    while ((opt = getopt(argc, argv, "v:t:m:h")) != -1) {
        switch (opt) {
            case 'v': cfg.num_veiculos = atoi(optarg); break;
            case 't': cfg.tick_ms      = atoi(optarg); break;
            case 'm': cfg.mapa_path    = optarg;       break;
            case 'h':
            default:
                fprintf(stderr, "Uso: ./simulador -v <veiculos> -t <tick_ms> [-m <mapa.txt>]\n");
                exit(opt == 'h' ? EXIT_SUCCESS : EXIT_FAILURE);
        }
    }

    if (cfg.num_veiculos <= 0 || cfg.tick_ms <= 0) {
        fprintf(stderr, "Erro: -v e -t são obrigatórios.\n");
        fprintf(stderr, "Uso: ./simulador -v <veiculos> -t <tick_ms> [-m <mapa.txt>]\n");
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