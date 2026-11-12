#!/usr/bin/env python3
"""
Script de análise de Speedup do MPI em relação ao código sequencial (04_convolucao.cpp).
Compila e executa a versão sequencial e a versão MPI variando o número de processos
de 1 até o total de processadores disponíveis na máquina, calcula o speedup e a
eficiência de cada execução e salva os resultados em um arquivo CSV.
"""

import argparse
import csv
import os
import re
import subprocess
import sys
from pathlib import Path

# Diretórios base do projeto (relativos à localização do script em scripts/)
ROOT_DIR = Path(__file__).resolve().parent.parent
SEQ_SRC = ROOT_DIR / "codigos" / "sequencial" / "04_convolucao.cpp"
SEQ_BIN = ROOT_DIR / "codigos" / "sequencial" / "04_convolucao"
MPI_SRC = ROOT_DIR / "codigos" / "mpi" / "convolucaoMPI.cpp"
MPI_BIN = ROOT_DIR / "codigos" / "mpi" / "convolucaoMPI"
DEFAULT_CSV = ROOT_DIR / "resultados" / "speedup_mpi.csv"


def compilar_codigos() -> None:
    """Compila os códigos sequencial e MPI com a flag de otimização -O2."""
    print("=== Compilando códigos ===")
    cmd_seq = ["g++", "-O2", "-o", str(SEQ_BIN), str(SEQ_SRC)]
    print(f"[Sequencial] {' '.join(cmd_seq)}")
    subprocess.run(cmd_seq, check=True)

    cmd_mpi = ["mpicxx", "-O2", "-o", str(MPI_BIN), str(MPI_SRC)]
    print(f"[MPI]        {' '.join(cmd_mpi)}")
    subprocess.run(cmd_mpi, check=True)
    print()


def extrair_dados_sequencial(output: str) -> dict:
    """
    Extrai os campos da saída do código sequencial:
    Exemplo: N=2000 passes=30  checksum=508998377.77  tempo=0.412345 s
    """
    padrao = (
        r"N=(\d+)\s+"
        r"passes=(\d+)\s+"
        r"checksum=([\d.]+)\s+"
        r"tempo=([\d.]+)\s*s"
    )
    match = re.search(padrao, output)
    if not match:
        raise ValueError(f"Não foi possível interpretar a saída do sequencial:\n{output}")

    return {
        "N": int(match.group(1)),
        "passes": int(match.group(2)),
        "checksum": float(match.group(3)),
        "tempo": float(match.group(4)),
    }


def extrair_dados_mpi(output: str) -> dict:
    """
    Extrai os campos da saída do código MPI:
    Exemplo: N=2000\tpasses=30\tprocs=4\tchecksum=508998377.77\ttempo=0.123456 s\tcomunicação=0.012345 s
    """
    padrao = (
        r"N=(\d+)\s+"
        r"passes=(\d+)\s+"
        r"procs=(\d+)\s+"
        r"checksum=([\d.]+)\s+"
        r"tempo=([\d.]+)\s*s\s+"
        r"comunica[cç][aã]o=([\d.]+)\s*s"
    )
    match = re.search(padrao, output)
    if not match:
        raise ValueError(f"Não foi possível interpretar a saída do MPI:\n{output}")

    return {
        "N": int(match.group(1)),
        "passes": int(match.group(2)),
        "procs": int(match.group(3)),
        "checksum": float(match.group(4)),
        "tempo": float(match.group(5)),
        "comunicacao": float(match.group(6)),
    }


def executar_sequencial(n: int, passes: int) -> dict:
    """Executa o binário sequencial e retorna os dados coletados."""
    print("=== Executando código Sequencial ===")
    cmd = [str(SEQ_BIN), str(n), str(passes)]
    res = subprocess.run(cmd, capture_output=True, text=True, check=True)
    saida = res.stdout.strip()
    print(f"Saída: {saida}\n")
    return extrair_dados_sequencial(saida)


def executar_mpi(np: int, n: int, passes: int) -> dict:
    """Executa o binário MPI para `np` processos e retorna os dados coletados."""
    cmd = [
        "mpirun",
        "--use-hwthread-cpus",
        "--oversubscribe",
        "-np",
        str(np),
        str(MPI_BIN),
        str(n),
        str(passes),
    ]
    res = subprocess.run(cmd, capture_output=True, text=True, check=True)
    saida = res.stdout.strip()
    print(f"[MPI np={np:2d}] {saida}")
    return extrair_dados_mpi(saida)


def salvar_csv(caminho_csv: Path, resultados: list[dict], modo: str = "w") -> None:
    """Salva a lista de resultados em um arquivo CSV."""
    caminho_csv.parent.mkdir(parents=True, exist_ok=True)
    colunas = [
        "versao",
        "processos",
        "N",
        "passes",
        "tempo_s",
        "tempo_comunicacao_s",
        "speedup",
        "eficiencia",
        "checksum",
    ]

    escrever_cabecalho = modo == "w" or not caminho_csv.exists() or caminho_csv.stat().st_size == 0

    with open(caminho_csv, mode=modo, newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=colunas)
        if escrever_cabecalho:
            writer.writeheader()
        for linha in resultados:
            writer.writerow(linha)


def main() -> None:
    max_procs_maquina = os.cpu_count() or 4

    parser = argparse.ArgumentParser(
        description="Análise de speedup do código MPI em comparação ao código sequencial para múltiplos tamanhos de N."
    )
    parser.add_argument(
        "--valores-n",
        nargs="+",
        type=int,
        default=[1000, 2000, 3000, 4000],
        help="Lista de valores de N para executar (padrão: 1000 2000 3000 4000)",
    )
    parser.add_argument(
        "-P",
        "--passes",
        type=int,
        default=30,
        help="Número de passes (padrão: 30)",
    )
    parser.add_argument(
        "--max-procs",
        type=int,
        default=max_procs_maquina,
        help=f"Número máximo de processos MPI (padrão detectado na máquina: {max_procs_maquina})",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=DEFAULT_CSV,
        help=f"Caminho do arquivo CSV de saída (padrão: {DEFAULT_CSV})",
    )
    args = parser.parse_args()

    compilar_codigos()

    # Prepara o arquivo CSV zerando execuções antigas
    salvar_csv(args.output, [], modo="w")

    todos_resultados = []

    for n in args.valores_n:
        print(f"\n==================================================")
        print(f"        INICIANDO TESTES PARA N = {n}, P = {args.passes}")
        print(f"==================================================")

        # 1. Executa o código sequencial
        seq_info = executar_sequencial(n, args.passes)
        tempo_seq = seq_info["tempo"]

        res_n = [
            {
                "versao": "sequencial",
                "processos": 1,
                "N": seq_info["N"],
                "passes": seq_info["passes"],
                "tempo_s": f"{tempo_seq:.6f}",
                "tempo_comunicacao_s": f"{0.0:.6f}",
                "speedup": f"{1.0:.4f}",
                "eficiencia": f"{1.0:.4f}",
                "checksum": f"{seq_info['checksum']:.2f}",
            }
        ]

        # 2. Executa o código MPI de 1 até max_procs
        print(f"\n=== Executando código MPI para N = {n} (1 até {args.max_procs} processos) ===")
        for np in range(1, args.max_procs + 1):
            mpi_info = executar_mpi(np, n, args.passes)
            tempo_mpi = mpi_info["tempo"]
            speedup = tempo_seq / tempo_mpi if tempo_mpi > 0 else 0.0
            eficiencia = speedup / np

            res_n.append(
                {
                    "versao": "mpi",
                    "processos": mpi_info["procs"],
                    "N": mpi_info["N"],
                    "passes": mpi_info["passes"],
                    "tempo_s": f"{tempo_mpi:.6f}",
                    "tempo_comunicacao_s": f"{mpi_info['comunicacao']:.6f}",
                    "speedup": f"{speedup:.4f}",
                    "eficiencia": f"{eficiencia:.4f}",
                    "checksum": f"{mpi_info['checksum']:.2f}",
                }
            )

        # Salva incrementalmente no CSV após concluir cada N
        salvar_csv(args.output, res_n, modo="a")
        todos_resultados.extend(res_n)

    # 3. Exibe resumo consolidado no terminal
    print("\n==================== RESUMO GERAL DE SPEEDUP ====================")
    print(f"{'Versão':<11} | {'N':>5} | {'Procs':>5} | {'Tempo (s)':>10} | {'Comun. (s)':>10} | {'Speedup':>8} | {'Eficiência':>10}")
    print("-" * 76)
    for r in todos_resultados:
        print(
            f"{r['versao']:<11} | {r['N']:>5} | {r['processos']:>5} | {r['tempo_s']:>10} | "
            f"{r['tempo_comunicacao_s']:>10} | {r['speedup']:>8} | {r['eficiencia']:>10}"
        )

    print(f"\nResultados consolidados com sucesso em: {args.output}")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        print(f"Erro ao executar comando: {e.cmd}", file=sys.stderr)
        if e.stdout:
            print(e.stdout, file=sys.stderr)
        if e.stderr:
            print(e.stderr, file=sys.stderr)
        sys.exit(1)

