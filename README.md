# Simulador de Tráfego Urbano em C

## Descrição

Este projeto consiste em uma simulação concorrente de tráfego urbano desenvolvida para a disciplina de Sistemas Operacionais. O objetivo é aplicar conceitos de concorrência, sincronização e prevenção de deadlocks em um ambiente onde veículos são representados por threads que competem por espaços em uma malha viária representada em ASCII.

## Integrantes

| Nome | GitHub |
| --- | --- |
| Elder Rayan Oliveira Silva | @eldrayan |
| Espedito Ramom Mascena Ricarto | @RamomRicarto |
| Manoel Junio Duarte da Silva | @Junio404 |
| Pedro Yan Alcantara Palácio | @pedropalacioo |
| Sabrina Alencar Soares | @sabrinaalencar|
| Samuel Wagner Tiburi Silveira | @samsilveira |
| Sebastião Sousa Soares | @SebastiaoSoares |

## Tecnologias e Mecanismos

- Linguagem: C
- Biblioteca: Pthreads
- Sincronização: Mutex, Semáforos e Variáveis de Condição
- Interface: Terminal (Visualização ASCII)

## Regras da Simulação

- Cada veículo é uma thread independente.
- Os veículos respeitam a sinalização de semáforos e a prioridade da ambulância.
- Não é permitida a ocupação simultânea de uma mesma célula (impenetrabilidade).
- O movimento é coordenado por um relógio global discreto (ticks).
- Implementação de estratégias para evitar deadlocks nos cruzamentos.

## Compilação e Execução

Em andamento
