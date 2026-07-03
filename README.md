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
| Sabrina Alencar Soares | @sabrinaalencar |
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

Para compilar e testar o simulador rapidamente, você pode usar as opções configuradas no `Makefile`.

### Comandos Rápidos

- **Compilar e executar com valores padrão (recomendado para testes):**

  ```bash
  make run
  ```

  *(Isso compilará o projeto e iniciará a simulação com 10 veículos e 100ms de tick).*

- **Apenas compilar:**

  ```bash
  make
  ```

- **Limpar arquivos gerados (.o e executável):**

  ```bash
  make clean
  ```

### Executando Manualmente

Após executar `make`, o executável `simulador` será gerado dentro da pasta `bin/`. O programa exige parâmetros obrigatórios para rodar:

- `-v`: Quantidade de carros (obrigatório)
- `-t`: Tempo de delay/tick em ms (obrigatório)
- `-m`: Caminho para o arquivo de mapa (opcional)

**Exemplo de uso:**

```bash
./bin/simulador -v 15 -t 100
```

Para exibir a mensagem de ajuda, execute:

```bash
./bin/simulador --help
```
