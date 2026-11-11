#!/usr/bin/env bash

# Mede static, dynamic e guided com diferentes equipes, valida cada execução
# contra o baseline sequencial e gera CSVs e gráficos.
#
# Uso: bash scripts/medir_openmp.sh [N] [P] [repeticoes] [aquecimentos]
# Exemplo: bash scripts/medir_openmp.sh 2000 30 5 2

set -euo pipefail

raiz="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$raiz"

N="${1:-2000}"
P="${2:-30}"
REPS="${3:-5}"
WARM="${4:-2}"

if ((N < 3 || N > 4048 || P < 1 || REPS < 1 || WARM < 0)); then
    echo "Erro: use 3 <= N <= 4048, P >= 1, repeticoes >= 1 e aquecimentos >= 0." >&2
    exit 1
fi

SRC_SEQ="codigos/sequencial/04_convolucao.cpp"
SRC_OMP="codigos/openmp/convolucaoOPM.cpp"
BIN_SEQ="build/convolucao_sequencial"
BIN_OMP="build/convolucao_openmp"
CSV="resultados/csv/openmp_medicoes.csv"

mkdir -p build resultados/csv resultados/graficos resultados/brutos

echo "Compilando baseline e OpenMP..."
g++ -O2 -Wall -Wextra "$SRC_SEQ" -o "$BIN_SEQ"
g++ -O2 -Wall -Wextra -fopenmp "$SRC_OMP" -o "$BIN_OMP"

extrair() {
    local chave="$1"
    local linha="$2"
    sed -n "s/.*${chave}=\\([^ ]*\\).*/\\1/p" <<< "$linha"
}

echo "versao,schedule,chunk,threads,N,P,rep,tempo_s,checksum,baseline_checksum,validado" > "$CSV"

echo "Aquecendo baseline..."
for ((r = 1; r <= WARM; ++r)); do
    "$BIN_SEQ" "$N" "$P" > "resultados/brutos/baseline_aquecimento_${r}.txt"
done

checksum_ref=""
echo "Medindo baseline..."
for ((r = 1; r <= REPS; ++r)); do
    linha="$("$BIN_SEQ" "$N" "$P")"
    tempo="$(extrair tempo "$linha")"
    checksum="$(extrair checksum "$linha")"

    if [[ -z "$tempo" || -z "$checksum" ]]; then
        echo "Erro ao interpretar a saída do baseline: $linha" >&2
        exit 1
    fi

    if [[ -z "$checksum_ref" ]]; then
        checksum_ref="$checksum"
    elif [[ "$checksum" != "$checksum_ref" ]]; then
        echo "Erro: o checksum do baseline variou entre execuções." >&2
        exit 1
    fi

    echo "sequencial,sequential,0,1,$N,$P,$r,$tempo,$checksum,$checksum_ref,true" >> "$CSV"
done

processadores="$(nproc)"
politicas=(static dynamic guided)
candidatas=(1 2 4 8 16)

for politica in "${politicas[@]}"; do
    for threads in "${candidatas[@]}"; do
        if ((threads > processadores)); then
            continue
        fi

        echo "Medindo schedule=$politica threads=$threads..."

        for ((r = 1; r <= WARM; ++r)); do
            OMP_DYNAMIC=FALSE OMP_NUM_THREADS="$threads" \
                "$BIN_OMP" "$N" "$P" "$politica" \
                > "resultados/brutos/openmp_${politica}_t${threads}_aquecimento_${r}.txt"
        done

        for ((r = 1; r <= REPS; ++r)); do
            linha="$(OMP_DYNAMIC=FALSE OMP_NUM_THREADS="$threads" \
                "$BIN_OMP" "$N" "$P" "$politica")"
            final="$(grep 'versao=openmp' <<< "$linha")"
            tempo="$(extrair tempo "$final")"
            checksum="$(extrair checksum "$final")"
            equipe="$(extrair threads "$final")"
            chunk="$(extrair chunk "$final")"

            if [[ -z "$tempo" || -z "$checksum" || -z "$equipe" || -z "$chunk" ]]; then
                echo "Erro ao interpretar a saída OpenMP: $linha" >&2
                exit 1
            fi

            validado="false"
            if [[ "$checksum" == "$checksum_ref" ]]; then
                validado="true"
            else
                echo "Erro: checksum OpenMP $checksum difere do baseline $checksum_ref." >&2
                exit 1
            fi

            echo "openmp,$politica,$chunk,$equipe,$N,$P,$r,$tempo,$checksum,$checksum_ref,$validado" >> "$CSV"
        done
    done
done

python3 scripts/analisar_openmp.py "$CSV"

echo
echo "Medições: resultados/csv/openmp_medicoes.csv"
echo "Resumo:   resultados/csv/openmp_resumo.csv"
echo "Speedup:  resultados/graficos/openmp_speedup.svg"
echo "Eficiência: resultados/graficos/openmp_eficiencia.svg"
