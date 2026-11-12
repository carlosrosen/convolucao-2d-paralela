#!/usr/bin/env python3
"""
Gera gráficos vetoriais SVG a partir do CSV de speedup MPI para comparar as métricas:
- Tempo de Execução Total (s)
- Tempo de Comunicação (s)
- Speedup (T_seq / T_mpi)
- Eficiência (Speedup / P)
em função do número de processos MPI para cada tamanho de N (1000, 2000, 3000, 4000).
"""

import argparse
import csv
from collections import defaultdict
from pathlib import Path

# Diretórios padrão
ROOT_DIR = Path(__file__).resolve().parent.parent
DEFAULT_CSV = ROOT_DIR / "resultados" / "speedup_mpi.csv"
DEFAULT_OUT_DIR = ROOT_DIR / "resultados" / "graficos"

CORES_N = {
    1000: "#2563eb",  # Azul
    2000: "#10b981",  # Verde
    3000: "#f59e0b",  # Âmbar / Laranja
    4000: "#ef4444",  # Vermelho
}
COR_PADRAO = ["#6366f1", "#8b5cf6", "#ec4899", "#14b8a6", "#06b6d4"]


def carregar_dados_csv(caminho_csv: Path) -> tuple[dict, dict]:
    """
    Lê o CSV e agrupa dados por N e versão.
    Retorna:
      - dados_mpi: dict[N, list[dict]] ordenado por processos
      - dados_seq: dict[N, float] (tempo sequencial de cada N)
    """
    if not caminho_csv.exists():
        raise FileNotFoundError(f"Arquivo CSV não encontrado: {caminho_csv}")

    dados_mpi = defaultdict(list)
    dados_seq = {}

    with open(caminho_csv, mode="r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            n = int(row["N"])
            versao = row["versao"].strip().lower()
            tempo = float(row["tempo_s"])
            tempo_comm = float(row["tempo_comunicacao_s"])
            speedup = float(row["speedup"])
            eficiencia = float(row["eficiencia"])
            processos = int(row["processos"])

            if versao == "sequencial":
                dados_seq[n] = tempo
            elif versao == "mpi":
                dados_mpi[n].append(
                    {
                        "processos": processos,
                        "tempo_s": tempo,
                        "tempo_comunicacao_s": tempo_comm,
                        "speedup": speedup,
                        "eficiencia": eficiencia,
                    }
                )

    # Ordena os pontos por quantidade de processos
    for n in dados_mpi:
        dados_mpi[n].sort(key=lambda d: d["processos"])

    return dict(dados_mpi), dados_seq


def obter_cor(n: int, idx: int = 0) -> str:
    """Retorna cor consistente para o valor de N."""
    if n in CORES_N:
        return CORES_N[n]
    return COR_PADRAO[idx % len(COR_PADRAO)]


def gerar_svg_grafico(
    titulo: str,
    eixo_y_label: str,
    series: list[dict],
    linhas_referencia: list[dict] | None = None,
    largura: int = 900,
    altura: int = 540,
) -> str:
    """
    Gera um gráfico SVG profissional e nítido sem depender de bibliotecas externas.
    series = [
        {"nome": "N=1000", "cor": "#2563eb", "pontos": [(x, y), ...]},
        ...
    ]
    linhas_referencia = [
        {"nome": "Ideal", "cor": "#94a3b8", "pontos": [(x, y), ...], "dash": "4,4"}
    ]
    """
    margin_left = 90
    margin_right = 160
    margin_top = 70
    margin_bottom = 65

    plot_w = largura - margin_left - margin_right
    plot_h = altura - margin_top - margin_bottom

    # Coleta limites dos eixos
    todos_x = []
    todos_y = []
    for s in series:
        for x, y in s["pontos"]:
            todos_x.append(x)
            todos_y.append(y)
    if linhas_referencia:
        for r in linhas_referencia:
            for x, y in r["pontos"]:
                todos_x.append(x)
                todos_y.append(y)

    if not todos_x or not todos_y:
        return "<svg></svg>"

    min_x = min(todos_x)
    max_x = max(todos_x)
    min_y = 0.0  # Começa no zero para comparações justas de métricas
    max_y = max(todos_y) * 1.10  # 10% de folga no topo

    if max_y <= 0:
        max_y = 1.0

    def escala_x(val: float) -> float:
        if max_x == min_x:
            return margin_left + plot_w / 2
        return margin_left + ((val - min_x) / (max_x - min_x)) * plot_w

    def escala_y(val: float) -> float:
        if max_y == min_y:
            return margin_top + plot_h / 2
        return margin_top + plot_h - ((val - min_y) / (max_y - min_y)) * plot_h

    svg_parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {largura} {altura}" '
        f'style="background: #ffffff; font-family: system-ui, -apple-system, sans-serif;">',
        '<defs>',
        '  <filter id="shadow" x="-5%" y="-5%" width="110%" height="110%">',
        '    <feDropShadow dx="0" dy="1" stdDeviation="1" flood-opacity="0.15"/>',
        '  </filter>',
        '</defs>',
    ]

    # Título do Gráfico
    svg_parts.append(
        f'<text x="{margin_left + plot_w / 2}" y="36" text-anchor="middle" '
        f'font-size="19" font-weight="700" fill="#0f172a">{titulo}</text>'
    )

    # Grid Horizontal e Rótulos Y
    num_ticks_y = 6
    for i in range(num_ticks_y + 1):
        y_val = min_y + (max_y - min_y) * (i / num_ticks_y)
        py = escala_y(y_val)
        svg_parts.append(
            f'<line x1="{margin_left}" y1="{py:.1f}" x2="{margin_left + plot_w}" y2="{py:.1f}" '
            f'stroke="#e2e8f0" stroke-width="1"/>'
        )
        texto_y = f"{y_val:.4f}" if max_y < 0.05 else (f"{y_val:.2f}" if max_y < 10 else f"{y_val:.1f}")
        svg_parts.append(
            f'<text x="{margin_left - 12}" y="{py + 4:.1f}" text-anchor="end" '
            f'font-size="12" fill="#64748b">{texto_y}</text>'
        )

    # Grid Vertical e Rótulos X (valores inteiros de processos)
    x_ticks = sorted(list(set(int(x) for x in todos_x)))
    for val_x in x_ticks:
        px = escala_x(val_x)
        svg_parts.append(
            f'<line x1="{px:.1f}" y1="{margin_top}" x2="{px:.1f}" y2="{margin_top + plot_h}" '
            f'stroke="#f1f5f9" stroke-width="1"/>'
        )
        svg_parts.append(
            f'<text x="{px:.1f}" y="{margin_top + plot_h + 20}" text-anchor="middle" '
            f'font-size="12" fill="#64748b">{val_x}</text>'
        )

    # Rótulos dos Eixos
    svg_parts.append(
        f'<text x="{margin_left + plot_w / 2}" y="{margin_top + plot_h + 46}" text-anchor="middle" '
        f'font-size="13" font-weight="600" fill="#334155">Número de Processos MPI</text>'
    )
    svg_parts.append(
        f'<text transform="rotate(-90)" x="-{margin_top + plot_h / 2}" y="26" text-anchor="middle" '
        f'font-size="13" font-weight="600" fill="#334155">{eixo_y_label}</text>'
    )

    # Linhas de referência (ex: Speedup Linear Ideal)
    if linhas_referencia:
        for ref in linhas_referencia:
            pts = ref["pontos"]
            d_path = " ".join(f"{'M' if idx==0 else 'L'} {escala_x(x):.1f} {escala_y(y):.1f}" for idx, (x, y) in enumerate(pts))
            dash = ref.get("dash", "4,4")
            cor = ref.get("cor", "#94a3b8")
            svg_parts.append(
                f'<path d="{d_path}" fill="none" stroke="{cor}" stroke-width="1.8" '
                f'stroke-dasharray="{dash}"/>'
            )

    # Séries de dados
    for s in series:
        cor = s["cor"]
        pts = s["pontos"]
        d_path = " ".join(f"{'M' if idx==0 else 'L'} {escala_x(x):.1f} {escala_y(y):.1f}" for idx, (x, y) in enumerate(pts))
        svg_parts.append(
            f'<path d="{d_path}" fill="none" stroke="{cor}" stroke-width="2.5" stroke-linecap="round"/>'
        )
        # Marcadores (círculos)
        for x, y in pts:
            px = escala_x(x)
            py = escala_y(y)
            svg_parts.append(
                f'<circle cx="{px:.1f}" cy="{py:.1f}" r="4.5" fill="{cor}" stroke="#ffffff" stroke-width="2" '
                f'filter="url(#shadow)"/>'
            )

    # Legenda à direita
    leg_x = margin_left + plot_w + 22
    leg_y_start = margin_top + 10
    svg_parts.append(
        f'<rect x="{leg_x - 10}" y="{leg_y_start - 10}" width="145" '
        f'height="{len(series) * 26 + (30 if linhas_referencia else 15)}" rx="6" '
        f'fill="#f8fafc" stroke="#e2e8f0" stroke-width="1"/>'
    )

    cur_y = leg_y_start + 6
    for s in series:
        cor = s["cor"]
        nome = s["nome"]
        svg_parts.append(f'<line x1="{leg_x}" y1="{cur_y}" x2="{leg_x + 18}" y2="{cur_y}" stroke="{cor}" stroke-width="3"/>')
        svg_parts.append(f'<circle cx="{leg_x + 9}" cy="{cur_y}" r="3.5" fill="{cor}"/>')
        svg_parts.append(
            f'<text x="{leg_x + 26}" y="{cur_y + 4}" font-size="12" font-weight="600" fill="#1e293b">{nome}</text>'
        )
        cur_y += 24

    if linhas_referencia:
        for ref in linhas_referencia:
            cor = ref.get("cor", "#94a3b8")
            nome = ref["nome"]
            dash = ref.get("dash", "3,3")
            svg_parts.append(
                f'<line x1="{leg_x}" y1="{cur_y}" x2="{leg_x + 18}" y2="{cur_y}" stroke="{cor}" '
                f'stroke-width="1.8" stroke-dasharray="{dash}"/>'
            )
            svg_parts.append(
                f'<text x="{leg_x + 26}" y="{cur_y + 4}" font-size="11" fill="#64748b">{nome}</text>'
            )
            cur_y += 24

    svg_parts.append("</svg>")
    return "\n".join(svg_parts)


def gerar_graficos_svg(dados_mpi: dict, dados_seq: dict, pasta_saida: Path) -> list[Path]:
    """Gera gráficos SVG para cada métrica comparando os valores de N."""
    pasta_saida.mkdir(parents=True, exist_ok=True)
    arquivos_gerados = []

    lista_n = sorted(dados_mpi.keys())
    procs_max = max(d["processos"] for n in lista_n for d in dados_mpi[n])

    # 1. Gráfico de Tempo de Execução Total
    series_tempo = []
    for idx, n in enumerate(lista_n):
        pts = [(d["processos"], d["tempo_s"]) for d in dados_mpi[n]]
        series_tempo.append({"nome": f"N = {n}", "cor": obter_cor(n, idx), "pontos": pts})

    # Referência do sequencial em tracejado para cada N (para 1 processo)
    svg_tempo = gerar_svg_grafico(
        titulo="Tempo de Execução MPI vs Número de Processos",
        eixo_y_label="Tempo de Execução (segundos)",
        series=series_tempo,
    )
    p_tempo = pasta_saida / "tempo_execucao_mpi.svg"
    p_tempo.write_text(svg_tempo, encoding="utf-8")
    arquivos_gerados.append(p_tempo)

    # 2. Gráfico de Tempo de Comunicação
    series_comm = []
    for idx, n in enumerate(lista_n):
        pts = [(d["processos"], d["tempo_comunicacao_s"]) for d in dados_mpi[n]]
        series_comm.append({"nome": f"N = {n}", "cor": obter_cor(n, idx), "pontos": pts})

    svg_comm = gerar_svg_grafico(
        titulo="Tempo de Comunicação (Troca de Halo) vs Processos",
        eixo_y_label="Tempo de Comunicação (segundos)",
        series=series_comm,
    )
    p_comm = pasta_saida / "tempo_comunicacao_mpi.svg"
    p_comm.write_text(svg_comm, encoding="utf-8")
    arquivos_gerados.append(p_comm)

    # 3. Gráfico de Speedup
    series_speedup = []
    for idx, n in enumerate(lista_n):
        pts = [(d["processos"], d["speedup"]) for d in dados_mpi[n]]
        series_speedup.append({"nome": f"N = {n}", "cor": obter_cor(n, idx), "pontos": pts})

    ref_speedup = [
        {
            "nome": "Linear Ideal",
            "cor": "#94a3b8",
            "pontos": [(1, 1.0), (procs_max, float(procs_max))],
            "dash": "5,5",
        }
    ]
    svg_speedup = gerar_svg_grafico(
        titulo="Speedup MPI vs Número de Processos (Sp = Tseq / Tpar)",
        eixo_y_label="Speedup (adimensional)",
        series=series_speedup,
        linhas_referencia=ref_speedup,
    )
    p_speedup = pasta_saida / "speedup_mpi.svg"
    p_speedup.write_text(svg_speedup, encoding="utf-8")
    arquivos_gerados.append(p_speedup)

    # 4. Gráfico de Eficiência
    series_eficiencia = []
    for idx, n in enumerate(lista_n):
        pts = [(d["processos"], d["eficiencia"]) for d in dados_mpi[n]]
        series_eficiencia.append({"nome": f"N = {n}", "cor": obter_cor(n, idx), "pontos": pts})

    ref_eficiencia = [
        {
            "nome": "Eficiência 100%",
            "cor": "#94a3b8",
            "pontos": [(1, 1.0), (procs_max, 1.0)],
            "dash": "4,4",
        }
    ]
    svg_eficiencia = gerar_svg_grafico(
        titulo="Eficiência Paralela MPI vs Número de Processos (Ep = Sp / P)",
        eixo_y_label="Eficiência (0.0 a 1.0)",
        series=series_eficiencia,
        linhas_referencia=ref_eficiencia,
    )
    p_eficiencia = pasta_saida / "eficiencia_mpi.svg"
    p_eficiencia.write_text(svg_eficiencia, encoding="utf-8")
    arquivos_gerados.append(p_eficiencia)

    return arquivos_gerados


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Gera gráficos SVG comparativos para cada métrica do CSV variando N."
    )
    parser.add_argument(
        "-i",
        "--input",
        type=Path,
        default=DEFAULT_CSV,
        help=f"Caminho do arquivo CSV de entrada (padrão: {DEFAULT_CSV})",
    )
    parser.add_argument(
        "-o",
        "--out-dir",
        type=Path,
        default=DEFAULT_OUT_DIR,
        help=f"Diretório de saída para os gráficos SVG (padrão: {DEFAULT_OUT_DIR})",
    )
    args = parser.parse_args()

    print(f"Lendo dados de: {args.input}")
    dados_mpi, dados_seq = carregar_dados_csv(args.input)
    print(f"Valores de N encontrados: {list(dados_mpi.keys())}")

    print("Gerando gráficos vetoriais SVG...")
    svgs = gerar_graficos_svg(dados_mpi, dados_seq, args.out_dir)
    for p in svgs:
        print(f"  -> {p}")

    print("\nTodos os gráficos SVG foram gerados com sucesso!")


if __name__ == "__main__":
    main()
