# Resume o CSV do baseline e gera os graficos
# Comando para rodar:  python3 scripts/resumir_baseline.py 

import sys
from pathlib import Path
import matplotlib
matplotlib.use("Agg")  # container sem tela
import matplotlib.pyplot as plt
import pandas as pd


def juntar_checksums(valores):
    return "|".join(sorted(valores.unique()))


entrada = sys.argv[1] if len(sys.argv) > 1 else "resultados/csv/baseline_sequencial.csv"
saida = sys.argv[2] if len(sys.argv) > 2 else "resultados/csv/baseline_resumo.csv"
grafico = sys.argv[3] if len(sys.argv) > 3 else "resultados/graficos/baseline_tempo.png"

if not Path(entrada).exists():
    sys.exit(f"Arquivo nao encontrado: {entrada}")

df = pd.read_csv(entrada, dtype={"checksum": str})
g = df.groupby(["N", "P"])

resumo = pd.DataFrame({
    "repeticoes": g.size(),
    "total_mediana_s": g["tempo_total_s"].median(),
    "total_min_s": g["tempo_total_s"].min(),
    "total_max_s": g["tempo_total_s"].max(),
    "total_desvio_s": g["tempo_total_s"].std(),
    "conv_mediana_s": g["tempo_conv_s"].median(),
    "conv_min_s": g["tempo_conv_s"].min(),
    "conv_max_s": g["tempo_conv_s"].max(),
    "conv_desvio_s": g["tempo_conv_s"].std(),
    "checksums": g["checksum"].agg(juntar_checksums),
}).reset_index()

colunas = [
    "N", "P", "repeticoes",
    "total_mediana_s", "total_min_s", "total_max_s", "total_desvio_s",
    "conv_mediana_s", "conv_min_s", "conv_max_s", "conv_desvio_s",
    "checksums",
]
resumo = resumo[colunas]
resumo[["total_desvio_s", "conv_desvio_s"]] = resumo[["total_desvio_s", "conv_desvio_s"]].fillna(0)

Path(saida).parent.mkdir(parents=True, exist_ok=True)
resumo.to_csv(saida, index=False, float_format="%.9f")

# Grafico
plt.figure(figsize=(8, 5))
plt.errorbar(
    resumo["N"], resumo["total_mediana_s"],
    yerr=[resumo["total_mediana_s"] - resumo["total_min_s"], resumo["total_max_s"] - resumo["total_mediana_s"]],
    marker="o", capsize=4, label="Tempo total",
)
plt.errorbar(
    resumo["N"], resumo["conv_mediana_s"],
    yerr=[resumo["conv_mediana_s"] - resumo["conv_min_s"], resumo["conv_max_s"] - resumo["conv_mediana_s"]],
    marker="s", capsize=4, label="Convolucao",
)
plt.xlabel("N (lado da imagem)")
plt.ylabel("Tempo (s)")
plt.title("Baseline sequencial (P=30)")
plt.xticks(resumo["N"])
plt.grid(True)
plt.legend()
plt.tight_layout()
Path(grafico).parent.mkdir(parents=True, exist_ok=True)
plt.savefig(grafico, dpi=150)
plt.close()
