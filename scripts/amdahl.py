# Calcula a Lei de Amdahl a partir do resumo do baseline.
#
# Uso:
#   python3 scripts/amdahl.py [resumo.csv] [saida.csv] [grafico.png]

import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")  # container sem tela
import matplotlib.pyplot as plt
import pandas as pd

entrada = sys.argv[1] if len(sys.argv) > 1 else "resultados/csv/baseline_resumo.csv"
saida = sys.argv[2] if len(sys.argv) > 2 else "resultados/csv/amdahl.csv"
grafico = sys.argv[3] if len(sys.argv) > 3 else "resultados/graficos/amdahl.png"

if not Path(entrada).exists():
    sys.exit(f"Arquivo nao encontrado: {entrada}")

df = pd.read_csv(entrada)

df["f"] = df["conv_mediana_s"] / df["total_mediana_s"]
df["B"] = 1 - df["f"] 
df["S_max"] = 1 / df["B"]

for n in (1, 2, 4, 8, 16):
    df[f"S{n}"] = 1 / (df["B"] + df["f"] / n)

colunas = ["N", "P", "f", "B", "S_max", "S1", "S2", "S4", "S8", "S16"]
amdahl = df[colunas].copy()
registros = amdahl.sort_values("N").to_dict("records")

Path(saida).parent.mkdir(parents=True, exist_ok=True)
amdahl.to_csv(saida, index=False, float_format="%.6f")

# Grafico comparando os N
ns = list(range(1, 17))
fig, eixos = plt.subplots(2, 2, figsize=(10, 7), sharex=True, sharey=True)

for ax, linha in zip(eixos.flat, registros):
    S = [1 / (linha["B"] + linha["f"] / n) for n in ns]
    ax.plot(ns, S, marker="o")
    ax.axhline(linha["S_max"], linestyle="--", color="gray")
    ax.set_title(f"N={int(linha['N'])}  (S_max = {linha['S_max']:.2f})")
    ax.set_xticks([1, 2, 4, 8, 16])
    ax.grid(True)

fig.suptitle("Lei de Amdahl")
fig.supxlabel("Threads (n)")
fig.supylabel("Speedup teorico S(n)")
fig.tight_layout()
Path(grafico).parent.mkdir(parents=True, exist_ok=True)
fig.savefig(grafico, dpi=150)
plt.close(fig)
