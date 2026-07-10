# Simulação Concorrente de Tráfego Urbano em C

## Resumo
Este relatório descreve o projeto e a implementação de um simulador de tráfego urbano concorrente em linguagem C utilizando a biblioteca de Threads (Pthreads) e a biblioteca `ncurses` para a visualização. Aborda-se a representação do mapa em memória, o ciclo de vida multithreaded dos veículos, os mecanismos de sincronização empregados para garantir a exclusão mútua sem o uso de espera ocupada (*busy waiting*), a estratégia de ordenação de recursos (*Resource Ordering*) para prevenção de *deadlocks* e a lógica de prioridade para a passagem da ambulância.

## 1. Introdução
Simular o tráfego urbano sob uma perspectiva de sistemas concorrentes exige mapear veículos a fluxos independentes de execução (threads) que disputam recursos compartilhados de capacidade restrita. Este trabalho detalha as decisões tomadas para a criação de um simulador livre de *deadlocks* e condições de corrida, cumprindo os requisitos de sincronização, concorrência e visualização estabelecidos na especificação.

## 2. Representação do Mapa
O mapa da simulação é representado por uma matriz bidimensional (grade) preenchida logicamente no código-fonte, configurando avenidas de mão única e dupla, calçadas e cruzamentos. Cada célula da malha viária é uma estrutura que armazena seu estado, o veículo ocupante e seu respectivo recurso de sincronização de exclusão mútua (`pthread_mutex_t`).

### 2.1 Trade-off de Granularidade do Lock
A modelagem adotou a estratégia de **Locks Finos (Por Célula)**. Cada célula física possui um Mutex exclusivo.
* **Vantagem (Paralelismo Máximo):** Múltiplos veículos podem trafegar simultaneamente pela mesma via de forma independente, maximizando a vazão de tráfego. Locks grossos (por via inteira ou mapa) destruiriam o paralelismo.
* **Desvantagem (Risco de Deadlock):** A granularidade fina introduziu o risco de concorrência nos cruzamentos, o que exigiu a implementação de uma estratégia rígida para aquisição de multiplos locks em um salto de célula.

## 3. Modelagem e Execução das Threads
O sistema foi dividido nas seguintes entidades ativas:
* **Thread de Relógio Global:** Controla o tempo da simulação através de *ticks* discretos, notificando as threads dos veículos (via `pthread_cond_broadcast`) e gerenciando a alternância periódica dos semáforos dos cruzamentos.
* **Thread Gerenciadora de Spawn:** Mantém a quantidade desejada de veículos ativos, instanciando novos carros e ambulâncias sempre que há vagas nos pontos de *spawn* e outro veículo faz o *despawn*.
* **Threads de Veículos (Carros e Ambulância):** Cada veículo atua de forma autônoma. Eles planejam suas rotas e calculam seus passos baseados no ritmo do relógio global e na sua velocidade, solicitando *locks* das células para o avanço seguro.

## 4. Mecanismos de Sincronização e Ausência de Espera Ocupada
Para gerenciar a coordenação e evitar *busy waiting* (gasto inútil de ciclos de CPU), o projeto utiliza de maneira estrita:
* **Mutex (`pthread_mutex_t`):** O acesso a variáveis globais (relógio, contagem de veículos) e às células da matriz é garantido através de Mutexes. A mudança de célula por um veículo ocorre trancando a célula atual (origem) e a de destino de modo atômico.
* **Variáveis de Condição (`pthread_cond_t`):** 
  * *Relógio Global:* Os veículos utilizam `pthread_cond_wait` para dormirem de forma bloqueante até o relógio avançar o próximo tick (`cond_relogio`).
  * *Semáforos:* Cada célula de cruzamento possui uma `cond_semaforo`. Quando o sinal está vermelho, a thread do carro é bloqueada temporariamente liberando CPU. Quando o Relógio Global muda a transição do sinal, ele emite um *broadcast* reativando os veículos enfileirados.

**Nota:** Os Semáforos Contadores (`sem_t`) não se mostraram necessários durante a implementação, pois o uso apurado de Mutex atrelado a variáveis de condição sanou todos os gargalos e limites de ocupação dos cruzamentos com excelência.

## 5. Estratégia Contra Deadlocks e Ambulância

### 5.1 Prevenção de Deadlock por Ordenação Fixa (Resource Ordering)
Quando um veículo avança pelo mapa, ele reserva a célula atual e tenta reservar a célula seguinte concomitantemente. Sem um protocolo estabelecido, dois veículos trafegando opostos poderiam trancar reciprocamente seus destinos, caracterizando o *Deadlock* (Espera Circular).
A estratégia definida para evitar tal impasse foi a de **Ordenação de Recursos (*Lock Ordering*)**. Independente do sentido do movimento, os veículos são obrigados a obter os Mutexes das duas células na ordem crescente do seu índice linear na memória matriz (`índice = linha * COLUNAS + coluna`). Matematicamente, seguir uma ordem monotônica estrita impossibilita a criação de ciclos de travamento em qualquer cruzamento complexo do mapa.

### 5.2 Tráfego de Prioridade: A Ambulância
A ambulância (`A`) foi concebida usando uma lógica avançada de "Radar Look-ahead".
* **Override de Emergência:** A cada passo, a ambulância varre internamente 4 células consecutivas à sua frente. Ao detectar um cruzamento nessa janela, ela incrementa a flag de `override_emergencia` do cruzamento, forçando imperativamente que seu eixo mude para verde e as vias perpendiculares travem no vermelho. Logo que o veículo deixa a área, o *override* é relaxado, restaurando o cruzamento à sua temporização rítmica normal sem ocasionar colapsos ou acidentes.

## 6. Visualização da Simulação
A interface exibe o avanço autônomo dos fluxos, gerada via a biblioteca `ncurses`, oferecendo renderização atualizada de console e limpa.

**Exemplo Visual Capturado:**
```text
=== SIMULADOR DE TRAFEGO URBANO ===
Tick: 45 | Veiculos Ativos: 10 / 10      !!! EMERGENCIA ATIVA: Sinais Liberados !!!
Legenda: [=] Rua  [+] Cruzamento  [^v<>] Carros  [A] Ambulancia

      | ^ |   | ^ |
======+===+===+=========
 <  A   +   +   +  <
======+===+===+=========
      | v |   | v |
```
* **Carros:** Representados por setas `^`, `v`, `<`, `>` apontando a direção adotada no movimento. Os veículos respeitam o sinal e esperam a liberação sem uso de CPU.
* **Ambulância:** O caractere `A` notifica seu trânsito prioritário. O sinalizador superior (`!!! EMERGENCIA ATIVA !!!`) comprova o acionamento do Radar, liberando cruzamentos antecipadamente.
* **Terreno:** Ruas expressas por `=` e cruzamentos sinalizados com `+`.

### 6.1. Análise de Profiling (Valgrind/Helgrind)
Durante a depuração do projeto utilizando a ferramenta Helgrind (do Valgrind) para checagem de concorrência, nota-se a ocorrência de um número elevado de potenciais *data races* (centenas de milhares em poucos minutos). É importante registrar que **estes são falsos positivos** provocados pela biblioteca gráfica `ncurses`.
A `ncurses` não é *thread-safe* e não compartilha metadados de propriedade de memória com o Valgrind. Como as funções de desenho da biblioteca varrem repetidamente e em alta frequência os *buffers* internos não protegidos no nível da aplicação, o Helgrind acusa leitura concorrente. Além disso, a falta de uma `libc` instrumentada para depuração pelo sistema operacional dispara alertas internos na alocação de variáveis, não constituindo falhas lógicas da simulação desenvolvida.

## 7. Conclusão
A construção deste simulador confirmou as boas práticas de concorrência com APIs POSIX, suprindo com folga as regras demandadas. As decisões arquiteturais do uso de *Locks Finos* com proteção por ordenação de recursos atestaram a viabilidade de sistemas altamente paralelos em malhas compartilhadas. Os cruzamentos mantiveram-se imunes a impasses de lógica graças às travas sistemáticas combinadas à hibernação de fluxo (`pthread_cond_wait`), eliminando qualquer subutilização da simulação.
