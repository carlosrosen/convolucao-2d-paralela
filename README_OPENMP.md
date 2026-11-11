# Versão OpenMP

## Compilação e teste

```bash
make openmp
make test
```

Execução sobre a imagem sintética com quatro threads:

```bash
OMP_NUM_THREADS=4 ./build/convolucao_openmp 2000 30 static
```

O terceiro argumento aceita `static`, `dynamic` ou `guided`. A implementação
usa `schedule(runtime)` para trocar a política sem recompilar.

## Comparação visual

O alvo abaixo separa `foto.jpg` nos canais vermelho, verde e azul, executa dez
passes do filtro em cada canal com quatro threads nas três políticas e cria o
painel colorido comparativo:

```bash
make comparar-imagens THREADS=4 P_IMAGEM=10 TAMANHO=512
```

É possível escolher outra imagem:

```bash
make comparar-imagens IMAGEM=caminho/imagem.png THREADS=8 P_IMAGEM=20
```

Os resultados ficam em `resultados/imagens/`:

- `comparacao_politicas.png`: original e resultados lado a lado;
- `static_tN.png`, `dynamic_tN.png` e `guided_tN.png`: resultados separados;
- `comparacao.csv`: tempo, checksum e validação de cada política.

As três políticas devem gerar exatamente os mesmos pixels. O script interrompe
a execução se encontrar uma diferença. O escalonamento muda a distribuição do
trabalho e o tempo de execução, mas não deve mudar o resultado do filtro.

## Medições de desempenho

```bash
make medir-openmp N=2000 P=30 REPS=5 WARM=2
```

Esse alvo mede o baseline e a versão OpenMP, valida os checksums e atualiza os
CSVs e gráficos de speedup e eficiência em `resultados/`.
