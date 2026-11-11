#!/usr/bin/env python3

"""Resume as medições OpenMP e gera curvas de speedup e eficiência."""

import csv
import html
import statistics
import sys
from collections import defaultdict
from pathlib import Path


entrada = Path(sys.argv[1] if len(sys.argv) > 1 else
               "resultados/csv/openmp_medicoes.csv")
saida = Path("resultados/csv/openmp_resumo.csv")
grafico_speedup = Path("resultados/graficos/openmp_speedup.svg")
grafico_eficiencia = Path("resultados/graficos/openmp_eficiencia.svg")

if not entrada.exists():
    raise SystemExit(f"Arquivo não encontrado: {entrada}")

with entrada.open(newline="", encoding="utf-8") as arquivo:
    linhas = list(csv.DictReader(arquivo))

baseline = [float(linha["tempo_s"]) for linha in linhas
            if linha["versao"] == "sequencial"]
if not baseline:
    raise SystemExit("O CSV não contém medições do baseline.")

tempo_baseline = statistics.median(baseline)
grupos = defaultdict(list)

for linha in linhas:
    if linha["versao"] == "openmp":
        chave = (linha["schedule"], int(linha["threads"]),
                 int(linha["N"]), int(linha["P"]))
        grupos[chave].append(linha)

registros = []
for (politica, threads, n, passes), medicoes in sorted(grupos.items()):
    tempos = [float(linha["tempo_s"]) for linha in medicoes]
    mediana = statistics.median(tempos)
    speedup = tempo_baseline / mediana
    eficiencia = speedup / threads
    validado = all(linha["validado"] == "true" for linha in medicoes)

    registros.append({
        "schedule": politica,
        "chunk": medicoes[0]["chunk"],
        "threads": threads,
        "N": n,
        "P": passes,
        "repeticoes": len(tempos),
        "baseline_mediana_s": tempo_baseline,
        "openmp_mediana_s": mediana,
        "openmp_min_s": min(tempos),
        "openmp_max_s": max(tempos),
        "speedup": speedup,
        "eficiencia": eficiencia,
        "validado": str(validado).lower(),
    })

saida.parent.mkdir(parents=True, exist_ok=True)
campos = list(registros[0]) if registros else []
with saida.open("w", newline="", encoding="utf-8") as arquivo:
    escritor = csv.DictWriter(arquivo, fieldnames=campos)
    escritor.writeheader()
    escritor.writerows(registros)

por_politica = defaultdict(list)
for registro in registros:
    por_politica[registro["schedule"]].append(registro)

grafico_speedup.parent.mkdir(parents=True, exist_ok=True)


def gerar_svg(caminho, titulo, rotulo_y, campo, percentual=False):
    """Gera um gráfico de linhas em SVG sem dependências externas."""
    largura, altura = 900, 550
    esquerda, direita, topo, base = 90, 30, 65, 75
    area_x = largura - esquerda - direita
    area_y = altura - topo - base
    equipes = sorted({item["threads"] for item in registros})
    series = {}

    for politica, valores in sorted(por_politica.items()):
        valores.sort(key=lambda item: item["threads"])
        fator = 100 if percentual else 1
        series[politica] = [(item["threads"], fator * item[campo])
                            for item in valores]

    ideal = [(threads, 100 if percentual else threads)
             for threads in equipes]
    todos_y = [y for valores in series.values() for _, y in valores]
    todos_y.extend(y for _, y in ideal)
    max_y = max(todos_y) * 1.10 if todos_y else 1
    if percentual:
        max_y = max(max_y, 110)

    def px(thread):
        indice = equipes.index(thread)
        divisor = max(1, len(equipes) - 1)
        return esquerda + area_x * indice / divisor

    def py(valor):
        return topo + area_y * (1 - valor / max_y)

    cores = {"static": "#2563eb", "dynamic": "#dc2626",
             "guided": "#16a34a", "ideal": "#6b7280"}
    partes = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{largura}" '
        f'height="{altura}" viewBox="0 0 {largura} {altura}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{largura / 2}" y="32" text-anchor="middle" '
        f'font-family="sans-serif" font-size="22" font-weight="bold">'
        f'{html.escape(titulo)}</text>',
    ]

    for passo in range(6):
        valor = max_y * passo / 5
        y = py(valor)
        partes.append(f'<line x1="{esquerda}" y1="{y:.1f}" '
                       f'x2="{largura-direita}" y2="{y:.1f}" '
                       'stroke="#e5e7eb"/>')
        partes.append(f'<text x="{esquerda-10}" y="{y+4:.1f}" '
                       'text-anchor="end" font-family="sans-serif" '
                       f'font-size="12">{valor:.1f}</text>')

    partes.append(f'<line x1="{esquerda}" y1="{topo}" x2="{esquerda}" '
                   f'y2="{altura-base}" stroke="black"/>')
    partes.append(f'<line x1="{esquerda}" y1="{altura-base}" '
                   f'x2="{largura-direita}" y2="{altura-base}" stroke="black"/>')

    for thread in equipes:
        x = px(thread)
        partes.append(f'<text x="{x:.1f}" y="{altura-base+24}" '
                       'text-anchor="middle" font-family="sans-serif" '
                       f'font-size="13">{thread}</text>')

    def desenhar_serie(nome, valores, tracejado=False):
        pontos = " ".join(f"{px(x):.1f},{py(y):.1f}" for x, y in valores)
        estilo = ' stroke-dasharray="7 5"' if tracejado else ""
        partes.append(f'<polyline points="{pontos}" fill="none" '
                       f'stroke="{cores[nome]}" stroke-width="2.5"{estilo}/>')
        for x, y in valores:
            partes.append(f'<circle cx="{px(x):.1f}" cy="{py(y):.1f}" '
                           f'r="4" fill="{cores[nome]}"/>')

    for nome, valores in series.items():
        desenhar_serie(nome, valores)
    desenhar_serie("ideal", ideal, True)

    partes.append(f'<text x="{esquerda + area_x/2}" y="{altura-20}" '
                   'text-anchor="middle" font-family="sans-serif" '
                   'font-size="15">Número de threads</text>')
    partes.append(f'<text x="22" y="{topo + area_y/2}" '
                   'text-anchor="middle" font-family="sans-serif" '
                   f'font-size="15" transform="rotate(-90 22 {topo + area_y/2})">'
                   f'{html.escape(rotulo_y)}</text>')

    legenda_x = esquerda + 10
    for indice, nome in enumerate([*sorted(series), "ideal"]):
        x = legenda_x + indice * 120
        partes.append(f'<line x1="{x}" y1="52" x2="{x+24}" y2="52" '
                       f'stroke="{cores[nome]}" stroke-width="3"/>')
        partes.append(f'<text x="{x+30}" y="56" font-family="sans-serif" '
                       f'font-size="13">{nome}</text>')

    partes.append("</svg>")
    caminho.write_text("\n".join(partes), encoding="utf-8")


gerar_svg(grafico_speedup, "Convolução 2D — curva de speedup",
          "Speedup (T sequencial / T paralelo)", "speedup")
gerar_svg(grafico_eficiencia, "Convolução 2D — eficiência OpenMP",
          "Eficiência (%)", "eficiencia", percentual=True)

print(f"Baseline mediano: {tempo_baseline:.9f} s")
for item in registros:
    print(f"{item['schedule']:>7}  threads={item['threads']:>2}  "
          f"tempo={item['openmp_mediana_s']:.9f} s  "
          f"speedup={item['speedup']:.3f}  "
          f"eficiência={100 * item['eficiencia']:.1f}%")
