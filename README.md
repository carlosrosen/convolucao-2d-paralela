# Convolução 2D Paralela

Projeto da disciplina de Programação Paralela, 2026/2.
Problema: filtro de convolução 2D (blur 3x3)

## Sobre o projeto

O projeto parte de um código sequencial fornecido pelo professor, que aplica um filtro de convolução 2D em uma imagem sintética. Para cada pixel interno, o código calcula a média dos nove pixels da vizinhança 3x3, suavizando a imagem.

O trabalho consiste em paralelizar esse código sem alterar a lógica de cálculo nem a entrada. As versões paralelas devem produzir o mesmo resultado do código sequencial, que serve como gabarito, e devem executar em menos tempo.

## O que será desenvolvido

- Versão com OpenMP, para memória compartilhada.
- Versão com MPI, para memória distribuída, com decomposição de domínio e troca de halo.
- Medição do baseline, perfilamento e estimativa de speedup pela Lei de Amdahl.
- Análise experimental com tempos, speedup, eficiência e escalabilidade.

## Como executar o baseline

O script mede o tempo do código sequencial para `N = 1000, 2000, 3000 e 4000`, com `P = 30`, 2 execuções de aquecimento e 5 medições por tamanho.

Com Docker:

```bash
docker compose build
docker compose run --rm --no-TTY pp-lab bash scripts/medir_baseline.sh
```

Se o container já estiver em execução:

```bash
docker compose exec pp-lab bash scripts/medir_baseline.sh
```

O script pode ser chamado de qualquer diretório, ele muda para a raiz do projeto antes de compilar e medir.
Os parâmetros `P`, `WARM`, `REPS` e `SIZES` ficam no topo do `scripts/medir_baseline.sh`.

### Saídas

- `resultados/csv/baseline_sequencial.csv` — uma linha por execução
- `resultados/csv/baseline_resumo.csv` — medianas, mínimos, máximos e desvios por `N`
- `resultados/csv/amdahl.csv` — `f`, `B`, `S_max`, `S1`, `S2`, `S4`, `S8` e `S16`
- `resultados/graficos/baseline_tempo.png`
- `resultados/graficos/amdahl.png`
- `resultados/brutos/` — logs completos de cada execução
