CXX       ?= g++
PYTHON    ?= python3
CXXFLAGS  ?= -O2 -Wall -Wextra
OMPFLAGS  ?= -fopenmp

BUILD_DIR := build
SEQ_SRC   := codigos/sequencial/04_convolucao.cpp
OMP_SRC   := codigos/openmp/convolucao_openmp.cpp
SEQ_BIN   := $(BUILD_DIR)/convolucao_sequencial
OMP_BIN   := $(BUILD_DIR)/convolucao_openmp

N     ?= 2000
P     ?= 30
REPS  ?= 5
WARM  ?= 2
IMAGEM ?= foto.jpg
THREADS ?= 4
P_IMAGEM ?= 10
TAMANHO ?= 512

.PHONY: all baseline openmp medir-openmp comparar-imagens test clean

all: baseline openmp

baseline: $(SEQ_BIN)

openmp: $(OMP_BIN)

$(BUILD_DIR):
	mkdir -p $@

$(SEQ_BIN): $(SEQ_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(OMP_BIN): $(OMP_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $< -o $@

medir-openmp: all
	bash scripts/medir_openmp.sh $(N) $(P) $(REPS) $(WARM)

comparar-imagens: openmp
	$(PYTHON) scripts/gerar_comparacao_imagem.py --imagem "$(IMAGEM)" --threads $(THREADS) --passes $(P_IMAGEM) --tamanho $(TAMANHO)

test:
	bash testes/teste_gcc.sh

clean:
	rm -rf $(BUILD_DIR)
