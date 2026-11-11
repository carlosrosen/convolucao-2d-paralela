#!/usr/bin/env bash

set -euo pipefail

raiz_projeto="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fonte="$raiz_projeto/codigos/openmp/convolucao_openmp.cpp"
diretorio_temporario="$(mktemp -d)"
executavel="$diretorio_temporario/convolucao_openmp"

trap 'rm -rf "$diretorio_temporario"' EXIT

if ! command -v g++ >/dev/null 2>&1; then
    echo "ERRO: g++ não está instalado ou não foi encontrado no PATH." >&2
    exit 1
fi

echo "Compilador encontrado: $(g++ --version | head -n 1)"

g++ -O2 -Wall -Wextra -fopenmp "$fonte" -o "$executavel"

saida="$(OMP_NUM_THREADS=2 "$executavel" 32 2)"
printf '%s\n' "$saida"

if ! grep -q 'checksum=' <<< "$saida"; then
    echo "ERRO: o programa não produziu o checksum esperado." >&2
    exit 1
fi

echo "Teste GCC/OpenMP concluído com sucesso."
