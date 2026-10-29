/* Baseline 04 — Filtro de convolucao 2D (blur 3x3) sobre imagem sintetica
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 * Aspecto interessante: decomposicao de dominio 2D; tratamento de bordas
 * (halo) quando paralelizado em MPI.
 *
 * Compilar: g++ -O2 -o 04_convolucao 04_convolucao.cpp
 * Executar: ./04_convolucao 2000 30   (lado da imagem; num. de passes)
 */

#include <cstdio> // O(1)  
#include <cstdlib> // O(1)
#include <vector> // O(1)
#include <ctime> // O(1)
using namespace std; // O(1)

// funcao que devolve o tempo atual em segundos. O(1)
static double agora() {
    timespec t; clock_gettime(CLOCK_MONOTONIC, &t);   // O(1)
    return t.tv_sec + t.tv_nsec * 1e-9;               // O(1)
}

// função principal do programa. O(1)
int main(int argc, char** argv) { 

    // le N da linha de comando, utiliza 2000 como valor padrao. O(1)
    int N = (argc > 1) ? atoi(argv[1]) : 2000;

    // le P da linha de comando, utiliza 30 como valor padrão. O(1)
    int P = (argc > 2) ? atoi(argv[2]) : 30;

    // aloca dois vetores de N² elementos do tipo float
    // o construtor de vector zera os elementos. 2*N² escritas -> O(N²)
    // ambos juntos ocupam 2 (dois vetores) * N² * sizeof(float) bytes
    vector<float> a(N*N), b(N*N);

    // inicializa os N² elementos do vetor a. O(N²)
    // cada iteração realiza operações de custo constante. O(1)
    // O(1) * O(N²) = O(N²)
    for (int i = 0; i < N*N; i++) a[i] = (float)((i * 131) % 256);

    // marca o inicio da medicao de tempo. O(1)
    double t0 = agora();

    // executa P vezes a convolução 2D
    // cada passo processa O(N²) elementos
    // P * O(N²) -> O(P * N²) no total
    // hotspot
    for (int p = 0; p < P; p++) {

        // percorre as linhas internas da matriz, desconsiderando as bordas
        // executa N-2 iterações por linha. O(N)
        // (N-2) * O(N) -> O(N²) 
        for (int y = 1; y < N-1; y++)
        
            // percorre as colunas internas de cada linha
            // executa N-2 iterações por linha. O(N)
            // O(N-2) * O(1) -> O(N)
            for (int x = 1; x < N-1; x++) {

                // acumula os valores dos vizinhos do elemento atual. O(1)
                float s = 0;

                // percorre as tres linhas do kernel 3x3. O(1)
                // dy varia de -1 a 1
                for (int dy = -1; dy <= 1; dy++)

                    // percorre as tres colunas do kernel. O(1)
                    // dx varia de -1 a 1
                    // total de 3 * 3 = 9 vizinhos por elemento
                    for (int dx = -1; dx <= 1; dx++)

                        // soma o valor do vizinho ao acumulador.
                        // sao realizadas 9 leituras e 9 adições por elemento. O(1)
                        s += a[(y+dy)*N + (x+dx)];

                // calcula a media dos 9 vizinhos e armazena o resultado em b. O(1)
                // apenas os elementos internos são atualizados, as bordas nao sao escritas.
                b[y*N + x] = s / 9.0f;
            } 

        // troca os recursos internos dos vetores a e b, sem copiar seus elementos
        // operação de custo constante. O(1).
        a.swap(b);
    }  

    // marca o final da medicao de tempo. O(1)
    double t1 = agora();

    // inicia o acumulador do checksum. O(1)
    double soma = 0;

    // percorre os N² elementos do vetor a para calcular o checksum. O(N²)
    // cada iteração realiza uma soma de custo constante O(1)
    // O(N²) * O(1) -> O(N²)
    for (int i = 0; i < N*N; i++) soma += a[i];

    // exibe as dimensões, o numero de passadas, o checksum e o tempo medido
    // custo considerado O(1), pois a quantidade de dados impressos é fixa
    printf("N=%d passes=%d  checksum=%.2f  tempo=%.6f s\n", N, P, soma, t1 - t0);

    // encerramento do programa. O(1)
    return 0;
}
