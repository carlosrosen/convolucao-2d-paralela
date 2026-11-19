# Convolução 2D Paralela

Projeto de Programação Paralela com implementação e análise de desempenho de filtro de convolução 2D (blur 3x3) em versões **Sequencial**, **OpenMP** (memória compartilhada) e **MPI** (memória distribuída).

---

## 1. Compilação

O projeto utiliza `Makefile` para compilar todos os módulos em `build/`:

```bash
make            # Compila todas as versões (sequencial, openmp, mpi)
make sequencial # Compila apenas os códigos sequenciais
make openmp     # Compila apenas a versão OpenMP
make mpi        # Compila apenas a versão MPI
make clean      # Remove a pasta build/
```

*Nota: limite seguro para matriz sintética: `N <= 4048` para evitar overflow de inteiros na inicialização.*

---

## 2. Executando os Códigos Principais

### Via Makefile (Recomendado)

Você pode passar parâmetros diretamente (`N`, `P`, `THREADS`, `SCHED`, `NP`):

```bash
# Sequencial (N=tamanho, P=iterações)
make run-sequencial N=2000 P=30

# OpenMP (THREADS=qtd threads, SCHED=static|dynamic|guided)
make run-openmp N=2000 P=30 THREADS=4 SCHED=static

# MPI (NP=número de processos)
make run-mpi N=2000 P=30 NP=4

# Executar todas as versões com os parâmetros padrão (N=2000, P=30, NP=4, THREADS=4)
make run
```

### Execução Direta dos Binários

Após compilar com `make`, os binários estarão em `build/`:

```bash
# 1. Sequencial base
./build/sequencial/04_convolucao <N> <P>
./build/sequencial/perfilamento <N> <P>      # Exibe tempos detalhados por etapa

# 2. OpenMP (define threads via variável de ambiente)
OMP_NUM_THREADS=4 ./build/openmp/convolucao_openmp <N> <P> <schedule>
# Exemplo: OMP_NUM_THREADS=4 ./build/openmp/convolucao_openmp 2000 30 dynamic

# 3. MPI (execução distribuída)
mpirun -np <NP> ./build/mpi/convolucaoMPI <N> <P>
# Exemplo: mpirun -np 4 ./build/mpi/convolucaoMPI 2000 30
```

---

## 3. Executando os Scripts de Automação e Análise (Python e Bash)

### Baseline Sequencial e Lei de Amdahl

Mede o baseline para $N \in \{1000, 2000, 3000, 4000\}$ com $P=30$, calcula estatísticas e projeta a aceleração teórica:

```bash
# Executa medições completas, gera CSVs e gráficos
bash scripts/medir_baseline.sh

# Ou execute os scripts Python individualmente:
python3 scripts/resumir_baseline.py [entrada.csv] [saida.csv] [grafico.png]
python3 scripts/amdahl.py [resumo.csv] [saida.csv] [grafico.png]
```

### Análise e Comparação OpenMP

Executa testes com diferentes quantidades de threads e políticas de escalonamento (`static`, `dynamic`, `guided`):

```bash
# Bateria de testes e geração de gráficos de speedup/eficiência
bash scripts/medir_openmp.sh <N> <P> <repeticoes> <aquecimentos>
# Exemplo: bash scripts/medir_openmp.sh 2000 30 5 2

# Processamento do CSV gerado para gerar SVG de speedup e eficiência:
python3 scripts/analisar_openmp.py resultados/csv/openmp_medicoes.csv

# Aplicação do filtro em imagem real (foto.jpg) nos canais RGB:
python3 scripts/gerar_comparacao_imagem.py --imagem foto.jpg --threads 4 --passes 10 --tamanho 512
```

### Análise e Gráficos MPI

Mede tempos de execução e comunicação variando o número de processos MPI contra o baseline sequencial:

```bash
# Bateria de testes de Speedup/Eficiência MPI (gera resultados/speedup_mpi.csv)
python3 scripts/analise_speedup_mpi.py --valores-n 1000 2000 3000 4000 -P 30 --max-procs 8

# Geração dos gráficos SVG comparativos (Tempo Total, Comunicação, Speedup, Eficiência)
python3 scripts/gerar_graficos_mpi.py -i resultados/speedup_mpi.csv -o resultados/graficos/
```

---

## 4. Execução em Ambiente Docker

Para garantir consistência com o ambiente avaliado:

```bash
# Construir a imagem
docker compose build

# Executar comandos dentro do container
docker compose run --rm pp-lab make
docker compose run --rm pp-lab make run N=1000 P=10
docker compose run --rm pp-lab bash scripts/medir_baseline.sh
docker compose run --rm pp-lab python3 scripts/analise_speedup_mpi.py

# Ou abrir um shell interativo no container:
docker compose run --rm pp-lab bash
```

---

## 5. Estrutura de Saídas

Os resultados gerados são organizados em:

- `resultados/csv/`: dados tabulares de medições, resumos estatísticos e Lei de Amdahl.
- `resultados/graficos/`: gráficos PNG/SVG de tempos, speedup, eficiência e comunicação.
- `resultados/imagens/`: imagens processadas (RGB) e painéis comparativos de escalonamento.
- `resultados/brutos/`: logs de saída das execuções individuais.
