# Mede o tempo do codigo sequencial para varios tamanhos de N,
# sempre com P fixo, e grava os resultados em CSV.
#
#  N = 1000, 2000, 3000 e 4000
#  P = 30 passadas
#  2 execuções de aquecimento por N (descartadas)
#  5 execuções medidas por N
#  afinidade no núcleo 0, se taskset estiver disponível
#
# Limite importante
# N <= 4048. 
# Acima disso, i*131 estoura int na inicialização do vetor a (comportamento indefinido). 
#
# Uso (a partir da raiz do projeto): 
#   bash scripts/medir_baseline.sh
#
# Saidas:
#   resultados/brutos/baseline_N*_P30_*.txt  (logs brutos)
#   resultados/csv/baseline_sequencial.csv   (uma linha por execucao)

set -euo pipefail
# -e aborta se qualquer comando falhar
# -u aborta se uma variavel nao definida for usada
# -o pipefail  um erro em qualquer parte do pipe faz o pipe falhar

# Caminhos de compilação
BIN=build/perfilamento                     # binario gerado 
SRC=codigos/sequencial/perfilamento.cpp    # código utilizado

# Caminhos de saida
DIR_BRUTOS=resultados/brutos               # logs completos de cada execução
CSV=resultados/csv/baseline_sequencial.csv # uma linha por medição
RESUMO=resultados/csv/baseline_resumo.csv  # estatisticas calculadas no final

# Parametros do experimento
P=30                                        # passadas da convolução
WARM=2                                      # execuções de aquecimento por N (que serão descartadas)
REPS=5                                      # execuções medidas por N
SIZES=(1000 2000 3000 4000)                 # lados da imagem (N) testados


# Diretorios de saida
# -p cria os diretorios se nao existirem e nao reclama se ja existirem
mkdir -p build "$DIR_BRUTOS" resultados/csv

#Compilacao
echo "Compilando $SRC..."
g++ -O2 -Wall -o "$BIN" "$SRC"


# Afinidade de CPU
# taskset fixa o processo em um nucleo, reduzindo migracao e ruído
# so usa se o comando existir e se a fixacao no nucleo 0 for permitida
if command -v taskset >/dev/null 2>&1 && taskset -c 0 true 2>/dev/null; then
    RUN=(taskset -c 0)                      # prefixo do comando, com afinidade
    echo "Afinidade: núcleo 0 (taskset)"
else
    RUN=()                                  # array vazio, roda sem afinidade
    echo "Aviso: taskset indisponível ou sem permissão; medindo sem afinidade."
fi

# Cabecalho do CSV
# Colunas: versão, N, P, repetição, tempo da convolução, tempo total e checksum.
echo "versao,N,P,rep,tempo_conv_s,tempo_total_s,checksum" > "$CSV"


# Loop principal
for N in "${SIZES[@]}"; do
    # Condição de segurança na inicialização para evitar o comportamento indefinido
    if (( N > 4048 )); then
        echo "Erro: N=$N excede o limite seguro de 4048." >&2
        echo "Acima disso, i*131 estoura int na inicialização." >&2
        exit 1
    fi

    echo
    echo "=== N=$N, P=$P ==="

    # Aquecimento (será descartado das estatísticas)
    # As execuções de aquecimento são guardadas em disco para teste, mas não entram no CSV e nem no resumo
    for ((r=1; r<=WARM; r++)); do
        LOG="$DIR_BRUTOS/baseline_N${N}_P${P}_aquecimento${r}.txt"
        "${RUN[@]}" "$BIN" "$N" "$P" > "$LOG"
        printf "  aquecimento %d: %s\n" "$r" "$(grep -m1 'checksum=' "$LOG")"
    done

    # Medições válidas
    CHECKSUM_REF=""                         # primeiro checksum de N, os demais devem ser iguais

    for ((r=1; r<=REPS; r++)); do
        LOG="$DIR_BRUTOS/baseline_N${N}_P${P}_r${r}.txt"
        "${RUN[@]}" "$BIN" "$N" "$P" > "$LOG"

        # Extrai os campos do log:
        # checksum
        # tempos de convolução e total
        CHECKSUM=$(sed -n 's/.*checksum=\([0-9.]*\).*/\1/p' "$LOG" | head -n1)
        CONV=$(awk '$1=="Convolucao" {print $2}' "$LOG")
        TOTAL=$(awk '$1=="Total" {print $2}' "$LOG")

        # Se algum campo veio vazio, o log está incompleto
        if [[ -z "$CHECKSUM" || -z "$CONV" || -z "$TOTAL" ]]; then
            echo "Erro: não foi possível extrair dados de $LOG" >&2
            exit 1
        fi

        # O checksum deve ser idêntico entre as execuções do mesmo N.
        if [[ -z "$CHECKSUM_REF" ]]; then
            CHECKSUM_REF="$CHECKSUM"        # primeira execução: guarda a referência
        elif [[ "$CHECKSUM" != "$CHECKSUM_REF" ]]; then
            echo "Erro: checksum mudou em N=$N (rep $r): $CHECKSUM != $CHECKSUM_REF" >&2
            exit 1
        fi

        # Grava uma linha no CSV e mostra no terminal.
        echo "sequencial,$N,$P,$r,$CONV,$TOTAL,$CHECKSUM" >> "$CSV"
        printf "  rep %d: conv=%.9f s  total=%.9f s  checksum=%s\n" \
            "$r" "$CONV" "$TOTAL" "$CHECKSUM"
    done
done


# Encerramento e resumo
echo
echo "Logs brutos: $DIR_BRUTOS/"
echo "CSV:         $CSV"
