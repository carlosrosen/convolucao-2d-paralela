# Alvos principais:
#   make            compila tudo
#   make run        executa tudo (sequencial + openmp + MPI)
#   make run-sequencial
#   make run-openmp
#   make run-mpi
#   make clean
#
# Variaveis padrões:
#   N=2000 P=30 NP=4 THREADS=4 SCHED=static OPT=-O2
#
# Exemplos:
#   make
#   make run N=1000 P=10
#   make run-openmp THREADS=8 SCHED=dynamic
#   make run-mpi NP=8
#   make OPT=-O3

CXX      ?= g++
MPICXX   ?= mpicxx
MPIRUN   ?= mpirun
OPT      ?= -O2
OMPFLAGS ?= -fopenmp
CXXFLAGS = -Wall $(OPT)

SRC_DIR   := codigos
BUILD_DIR := build

SEQ_SRCS := $(wildcard $(SRC_DIR)/sequencial/*.cpp)
OMP_SRCS := $(wildcard $(SRC_DIR)/openmp/*.cpp)
MPI_SRCS := $(wildcard $(SRC_DIR)/mpi/*.cpp)

SEQ_BINS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%,$(SEQ_SRCS))
OMP_BINS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%,$(OMP_SRCS))
MPI_BINS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%,$(MPI_SRCS))
BINS     := $(SEQ_BINS) $(OMP_BINS) $(MPI_BINS)

# Parametros de execucao padrão
N       ?= 2000
P       ?= 30
NP      ?= 4
THREADS ?= 4
SCHED   ?= static

.PHONY: all sequencial openmp mpi run run-sequencial run-openmp run-mpi clean help

all: $(BINS)

sequencial: $(SEQ_BINS)
openmp: $(OMP_BINS)
mpi: $(MPI_BINS)

$(BUILD_DIR)/sequencial/%: $(SRC_DIR)/sequencial/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/openmp/%: $(SRC_DIR)/openmp/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) -o $@ $<

$(BUILD_DIR)/mpi/%: $(SRC_DIR)/mpi/%.cpp
	@mkdir -p $(dir $@)
	$(MPICXX) $(CXXFLAGS) -o $@ $<

run: run-sequencial run-openmp run-mpi

run-sequencial: $(SEQ_BINS)
	@for bin in $(SEQ_BINS); do \
		echo "==> $$bin $(N) $(P)"; \
		./$$bin $(N) $(P); \
	done

run-openmp: $(OMP_BINS)
	@for bin in $(OMP_BINS); do \
		echo "==> OMP_NUM_THREADS=$(THREADS) $$bin $(N) $(P) $(SCHED)"; \
		OMP_NUM_THREADS=$(THREADS) ./$$bin $(N) $(P) $(SCHED); \
	done

run-mpi: $(MPI_BINS)
	@for bin in $(MPI_BINS); do \
		echo "==> $(MPIRUN) -np $(NP) $$bin $(N) $(P)"; \
		$(MPIRUN) -np $(NP) $$bin $(N) $(P); \
	done

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "Alvos:"
	@echo "  make                  compila todos os binarios em $(BUILD_DIR)/"
	@echo "  make openmp           compila apenas o binario OpenMP"
	@echo "  make run              executa todos (N=$(N) P=$(P) THREADS=$(THREADS) NP=$(NP))"
	@echo "  make run-sequencial   executa apenas os sequenciais"
	@echo "  make run-openmp       executa o OpenMP (OMP_NUM_THREADS=$(THREADS), $(SCHED))"
	@echo "  make run-mpi          executa apenas o MPI (mpirun -np $(NP))"
	@echo "  make clean            remove $(BUILD_DIR)/"
	@echo
	@echo "Exemplos: make run N=1000 P=10 | make run-openmp THREADS=8 SCHED=dynamic | make run-mpi NP=8"
