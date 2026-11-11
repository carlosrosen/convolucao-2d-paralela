#!/usr/bin/env python3
import argparse
import csv
import os
import re
import subprocess
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageOps


def obter_argumentos():
    parser = argparse.ArgumentParser(description="Compara o blur OpenMP por escalonamento")
    parser.add_argument("--imagem", default="foto.jpg")
    parser.add_argument("--binario", default="build/convolucao_openmp")
    parser.add_argument("--saida", default="resultados/imagens")
    parser.add_argument("--threads", type=int, default=4)
    parser.add_argument("--passes", type=int, default=10)
    parser.add_argument("--tamanho", type=int, default=512)
    return parser.parse_args()


def executar(binario, entrada, saida, politica, args):
    ambiente = os.environ.copy()
    ambiente["OMP_NUM_THREADS"] = str(args.threads)
    comando = [str(binario), str(args.tamanho), str(args.passes), politica,
               "--input", str(entrada), "--output", str(saida)]
    processo = subprocess.run(comando, env=ambiente, text=True,
                              capture_output=True, check=True)
    linha = processo.stdout.strip().splitlines()[-1]
    tempo = re.search(r"tempo=([0-9.]+)", linha)
    checksum = re.search(r"checksum=([0-9.]+)", linha)
    if not tempo or not checksum:
        raise RuntimeError(f"saida inesperada do programa: {linha}")
    return float(tempo.group(1)), float(checksum.group(1))


def criar_painel(imagens, rotulos, caminho):
    largura, altura = imagens[0].size
    cabecalho = 58
    painel = Image.new("RGB", (largura * len(imagens), altura + cabecalho), "white")
    desenho = ImageDraw.Draw(painel)
    for indice, (imagem, rotulo) in enumerate(zip(imagens, rotulos)):
        x = indice * largura
        painel.paste(imagem.convert("RGB"), (x, cabecalho))
        desenho.multiline_text((x + 10, 10), rotulo, fill="black", spacing=4)
    painel.save(caminho)


def main():
    args = obter_argumentos()
    raiz = Path.cwd()
    entrada = (raiz / args.imagem).resolve()
    binario = (raiz / args.binario).resolve()
    diretorio = (raiz / args.saida).resolve()
    diretorio.mkdir(parents=True, exist_ok=True)

    original = Image.open(entrada).convert("RGB")
    original = ImageOps.fit(original, (args.tamanho, args.tamanho),
                            method=Image.Resampling.LANCZOS)
    original.save(diretorio / "original.png")

    canais_entrada = []
    for nome, canal in zip(("r", "g", "b"), original.split()):
        caminho = diretorio / f"original_{nome}.pgm"
        canal.save(caminho)
        canais_entrada.append(caminho)

    imagens = [original]
    rotulos = [f"Original\n{args.tamanho} x {args.tamanho}"]
    registros = []
    referencia = None

    for politica in ("static", "dynamic", "guided"):
        png_saida = diretorio / f"{politica}_t{args.threads}.png"
        canais_saida = []
        tempo = 0.0
        checksum = 0.0

        # O programa C++ trabalha com uma matriz de intensidades. Cada canal RGB
        # é uma matriz independente, então o mesmo filtro é aplicado a R, G e B.
        for nome, canal_entrada in zip(("r", "g", "b"), canais_entrada):
            canal_saida = diretorio / f"{politica}_t{args.threads}_{nome}.pgm"
            tempo_canal, checksum_canal = executar(
                binario, canal_entrada, canal_saida, politica, args
            )
            tempo += tempo_canal
            checksum += checksum_canal
            canais_saida.append(Image.open(canal_saida).convert("L").copy())

        imagem = Image.merge("RGB", canais_saida)
        imagem.save(png_saida)

        identica = referencia is None or ImageChops.difference(referencia, imagem).getbbox() is None
        if not identica:
            raise RuntimeError(f"a imagem de {politica} difere das demais politicas")
        if referencia is None:
            referencia = imagem.copy()

        imagens.append(imagem)
        rotulos.append(f"{politica} | {args.threads} threads\n{args.passes} passes | {tempo:.6f} s")
        registros.append({
            "politica": politica,
            "threads": args.threads,
            "passes": args.passes,
            "tamanho": args.tamanho,
            "tempo_s": f"{tempo:.6f}",
            "checksum": f"{checksum:.2f}",
            "saida_identica": "sim",
            "arquivo": str(png_saida.relative_to(raiz)),
        })

    with (diretorio / "comparacao.csv").open("w", newline="", encoding="utf-8") as arquivo:
        escritor = csv.DictWriter(arquivo, fieldnames=registros[0].keys())
        escritor.writeheader()
        escritor.writerows(registros)

    comparacao = diretorio / "comparacao_politicas.png"
    criar_painel(imagens, rotulos, comparacao)
    print(f"Comparacao gerada: {comparacao.relative_to(raiz)}")
    print("As imagens coloridas static, dynamic e guided sao identicas.")


if __name__ == "__main__":
    main()
