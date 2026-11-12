/* 
 * Convolucao 2D (blur 3x3) paralela em MPI — Decomposicao em Grade 2D com Halo
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 * Baseado em 04_convolucao.cpp
 *
 * Compilar: mpicxx -O2 -o convolucaoMPI convolucaoMPI.cpp
 * Executar: mpirun -np <numeros de núcleos de processadores> ./convolucaoMPI 2000 30
 */

/*
  Versão 2.1 com implementação da decomosição de domínio com halo utilizando o formato de grade
*/

#include <cstdio>
#include <cstdlib>
#include <mpi.h>
#include <vector>

using namespace std;

// Calcula tamanho local e deslocamento inicial para divisao 1D balanceada
static void decomp1d(int N, int nprocs, int coord, int &start, int &count) {
  int base = N / nprocs; // pega quantos valores cada processo tem que rodar
  int resto = N % nprocs; // pega o resto da divisão
  count = base + (coord < resto ? 1 : 0); // ele retorna quantos indices o node terá que processar
  //  e incrementa a quantidade para os nodes que tiver a coordenada menor que o resto,
  //  assim todos o resto é distribuido uniformemente.
  start = coord * base + (coord < resto ? coord : resto); // calcula os saltos de indice de acordo com a base e a coordenada
  // e adiciona o deslocamento causado pela adição dos elementos sobresalentes da divisão.
}

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);

  int world_rank, world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  int N = (argc > 1) ? atoi(argv[1]) : 2000;
  int P = (argc > 2) ? atoi(argv[2]) : 30;

  // Criacao da topologia cartesiana 2D (grade de processos)
  int dims[2] = {0, 0};
  MPI_Dims_create(world_size, 2, dims); // pega a quantidade de nodes,
  //  e separa quantos nodes irão para cada dimensão (linha e coluna)

  // 0 proibe o circulo e 1 habilita o circulo
  int periods[2] = {0, 0}; // A periodicidade define se as bordas tem fim
  // ou se elas se interligam o outro lado formando uma "lista circular" na sua dimensão

  MPI_Comm cart_comm;
  MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 1, &cart_comm); // Cria a comunicação
  // do tipo cartesiana 2D.

  int cart_rank, coords[2];
  MPI_Comm_rank(cart_comm, &cart_rank); // ve qual é o rank que a maquina ficou dentro da rede e guarda o número.
  MPI_Cart_coords(cart_comm, cart_rank, 2, coords); //identifica pelo rank em qual coordenada a maquina esta no plano cartesiano.

  // Identifica processos vizinhos adjacentes (Norte/Sul no eixo Y, Oeste/Leste
  // no eixo X)
  int north, south, west, east;
  MPI_Cart_shift(cart_comm, 0, 1, &north, &south);
  MPI_Cart_shift(cart_comm, 1, 1, &west, &east);

  // Decomposicao do dominio 2D (N x N) entre os processos da grade
  int start_y, linhas_locais;
  int start_x, colunas_locais;
  // realiza a segmentação de indices para cada node, separando onde cada um inicia <start_> quantos tem que processar <local_n>
  decomp1d(N, dims[0], coords[0], start_y, linhas_locais);
  decomp1d(N, dims[1], coords[1], start_x, colunas_locais);

  // Alocacao local com borda fantasma (halo) de 1 celula em cada lado
  int largura_com_halo = colunas_locais + 2;
  int altura_com_halo = linhas_locais + 2;
  vector<float> a(altura_com_halo * largura_com_halo, 0.0f);
  vector<float> b(altura_com_halo * largura_com_halo, 0.0f);

  // Inicializacao identica ao sequencial usando as coordenadas globais
  for (int y = 1; y <= linhas_locais; y++) {
    int global_y = start_y + (y - 1);
    for (int x = 1; x <= colunas_locais; x++) {
      int global_x = start_x + (x - 1);
      int global_i = global_y * N + global_x;
      a[y * largura_com_halo + x] = (float)((global_i * 131) % 256);
    }
  }

  // Tipo derivado para envio/recebimento de uma coluna (incluindo as linhas de
  // halo)
  MPI_Datatype col_type;
  // aloca a quantidade de linhas da coluna em blocos de um de tamanho e a cada passo ele pula a quantidade exata de cada linha para acessar os indices da coluna
  MPI_Type_vector(altura_com_halo, 1, largura_com_halo, MPI_FLOAT, &col_type); 
  MPI_Type_commit(&col_type); // salva o tipo para o uso do MPI na rede

  // Limites locais de iteracao (preservando as bordas globais y=0, y=N-1, x=0,
  // x=N-1 intactas)
  int ly_begin = (start_y == 0) ? 2 : 1;
  int ly_end = (start_y + linhas_locais == N) ? linhas_locais - 1 : linhas_locais;
  int lx_begin = (start_x == 0) ? 2 : 1;
  int lx_end = (start_x + colunas_locais == N) ? colunas_locais - 1 : colunas_locais;

  MPI_Barrier(cart_comm); // espera todos os nodes chegarem a esse ponto
  double tempo_comm = 0;
  double t0 = MPI_Wtime();

  for (int p = 0; p < P; p++) {
    // 1. Troca de halos verticais (Norte / Sul) — linhas contiguas
    double inicio_tempo_comunicacao = MPI_Wtime();
    MPI_Sendrecv(&a[1 * largura_com_halo + 1], colunas_locais, MPI_FLOAT, north, 0,
                 &a[(linhas_locais + 1) * largura_com_halo + 1], colunas_locais, MPI_FLOAT, south, 0,
                 cart_comm, MPI_STATUS_IGNORE);

    MPI_Sendrecv(&a[linhas_locais * largura_com_halo + 1], colunas_locais, MPI_FLOAT, south, 1,
                 &a[0 * largura_com_halo + 1], colunas_locais, MPI_FLOAT, north, 1, cart_comm,
                 MPI_STATUS_IGNORE);

    // 2. Troca de halos horizontais (Oeste / Leste) — colunas incluindo os
    // cantos ja recebidos de Norte/Sul
    MPI_Sendrecv(&a[0 * largura_com_halo + 1], 1, col_type, west, 2,
                 &a[0 * largura_com_halo + colunas_locais + 1], 1, col_type, east, 2, cart_comm,
                 MPI_STATUS_IGNORE);

    MPI_Sendrecv(&a[0 * largura_com_halo + colunas_locais], 1, col_type, east, 3,
                 &a[0 * largura_com_halo + 0], 1, col_type, west, 3, cart_comm,
                 MPI_STATUS_IGNORE);
    tempo_comm += MPI_Wtime() - inicio_tempo_comunicacao;

    // 3. Calculo da convolucao 3x3 no subdominio local
    for (int y = ly_begin; y <= ly_end; y++) {
      for (int x = lx_begin; x <= lx_end; x++) {
        float s = 0.0f;
        for (int dy = -1; dy <= 1; dy++) {
          for (int dx = -1; dx <= 1; dx++) {
            s += a[(y + dy) * largura_com_halo + (x + dx)];
          }
        }
        b[y * largura_com_halo + x] = s / 9.0f;
      }
    }

    a.swap(b);
  }

  double t1 = MPI_Wtime();
  double local_time = t1 - t0;
  double max_time = 0.0;
  double max_time_comm = 0.0;
  MPI_Reduce(&local_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, cart_comm);
  MPI_Reduce(&tempo_comm, &max_time_comm, 1, MPI_DOUBLE, MPI_MAX, 0, cart_comm);

  // Soma local apenas das celulas reais do subdominio (sem contar o halo)
  double local_soma = 0.0;
  for (int y = 1; y <= linhas_locais; y++) {
    for (int lx = 1; lx <= colunas_locais; lx++) {
      local_soma += a[y * largura_com_halo + lx];
    }
  }

  double global_soma = 0.0;
  MPI_Reduce(&local_soma, &global_soma, 1, MPI_DOUBLE, MPI_SUM, 0, cart_comm);

  if (cart_rank == 0) {
    printf("N=%d\tpasses=%d\tprocs=%d\tchecksum=%.2f\ttempo=%.6f s\tcomunicação=%.6f s\n", N, P, world_size, global_soma,
           max_time, max_time_comm);
  }

  MPI_Type_free(&col_type);
  MPI_Comm_free(&cart_comm);
  MPI_Finalize();
  return 0;
}