/* Baseline 04 — Filtro de convolucao 2D (blur 3x3) sobre imagem sintetica
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 * Aspecto interessante: decomposicao de dominio 2D; tratamento de bordas
 * (halo) quando paralelizado em MPI.
 *
 * Compilar: g++ -O2 -o 04_convolucao 04_convolucao.cpp
 * Executar: ./04_convolucao 2000 30   (lado da imagem; num. de passes)
 */
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <ctime>
#include <omp.h>

using namespace std;

static double agora() {
    timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(int argc, char** argv) {

    /*----------------------------------------------------------------------*/

    // Obtendo informações sobre o ambiente OpenMP

    /*----------------------------------------------------------------------*/

    // Número de processadores lógicos disponíveis,
    
    int processadores = omp_get_num_procs();

    // Número máximo de threads configurado no ambiente OpenMP,

    int max_threads = omp_get_max_threads();

    /*
        Variável para armazenar o número de threads 
        efetivamente usadas no programa, 
    */

    int threads_usadas = 0;

    //Saída de informações sobre os recursos disponíveis 

    printf("Processadores logicos disponíveis: %d\n", processadores);
    printf("Maximo de threads configurado: %d\n", max_threads);


    /*
        Aqui ocorre a paralelização da inicialização da imagem de entrada
        e a criação de uma equipe de threads para executar o código 
        dentro do bloco paralelo.
    */
    
    #pragma omp parallel
    {

        // Obtendo o ID da thread atual

        int id = omp_get_thread_num();
        int total = omp_get_num_threads();

        // Obtendo o número total de threads na equipe

        #pragma omp single
        {
            threads_usadas = omp_get_num_threads();
            printf("Threads efetivamente usadas: %d\n", threads_usadas);
        }

        // Impressão ordenada das threads

        for (int vez = 0; vez < total; ++vez) {

            // Sincroniza as threads para manter a impressão em ordem por ID.

            #pragma omp barrier

            if (id == vez) {
                printf("Thread ativa: %d\n", id);
            }
        }
    }   

    /*----------------------------------------------------------------------*/

    //Inicialização de variáveis e alocação de memória para as imagens

    /*----------------------------------------------------------------------*/

    /*
        N = Largira e altura da imagem
        P = Quantidade de passes do filtro
    */
    int N = (argc > 1) ? atoi(argv[1]) : 2000;
    int P = (argc > 2) ? atoi(argv[2]) : 30;

    /*
        a = Imagem de entrada
        b = Imagem que recebe o resultado do filtro
    */
    vector<float> a(N*N), b(N*N);


    /*
        Aqui ocorre a paralelização da inicialização da imagem de entrada.
    */
    
    #pragma omp parallel for schedule(static)

        /*
        Cada pixel recebe um valor entre 0 e 255.
        Isso simula uma imagem em tons de cinza
        sem precisar carregar um arquivo.
        */

        for (int i = 0; i < N * N; ++i) {
            a[i] = static_cast<float>((i * 131) % 256);
        }

    double t0 = agora();

    /*----------------------------------------------------------------------*/

    //Aplicação do filtro de convolução 2D (blur 3x3) sobre a imagem

    /*----------------------------------------------------------------------*/

    for (int p = 0; p < P; p++) {

        /*
            Aqui a diretiva pragma tem como seu diferencial a clausula
            collapse(2) junta dois laços alinhados em um único espaço 
            de iterações para o OpenMP distribuir entre as threads.

            Evitando assim que uma thread fique 
            ociosa enquanto outra ainda está processando.
        */

        #pragma omp parallel for collapse(2) schedule(static)

        /*
            Percorre então os pixels internos.
        */
        for (int y = 1; y < N-1; y++)
            for (int x = 1; x < N-1; x++) {

                /*
                    Imagine então o filtro de desfoque como uma matriz 3x3
                    Agora pergunte-se, o que dx e dy são?

                    dx -> deslocamento horizontal (x) do pixel central
                    dy -> deslocamento vertical (y) do pixel central

                    seus valores variam de -1 a 1, sendo:

                    -1: esquerda
                     0: central (Seja a mesma coluna(dx) ou linha(dy))
                     1: direita

                     assim forma-se esse exemplo de matriz

                    (-1,-1)  (0,-1)  (1,-1)
                    (-1, 0)  (0, 0)  (1, 0)
                    (-1, 1)  (0, 1)  (1, 1)

                    logo sabendo disso, o for duplo faz a deslocação por essa
                    matriz 3x3.
                */
                float s = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)

                    /* Aqui acontece a soma dos pixels vizinhos a cerca do pixel alvo
                       Pense como(ele soma o pixel alvo e os oito pixels 
                       ao redor formando uma região 3x3 com nove valores)

                       vizinho  vizinho  vizinho
                       vizinho  alvo     vizinho
                       vizinho  vizinho  vizinho

                       e o valor dessa soma é armazenado na variável s
                    */
                        s += a[(y+dy)*N + (x+dx)];

                /* Aqui acontece a média dos pixels vizinhos a cerca do pixel alvo 
                    dividida pelo número total de pixels somados 
                    
                    B então recebe o valor da média, 
                    que é o resultado do filtro de desfoque
                    */

                b[y*N + x] = s / 9.0f;
            }

        /* Aqui acontece a troca de ponteiros entre a e b
            para que a próxima iteração do filtro seja aplicada
            sobre a imagem resultante do filtro anterior.
        */
        a.swap(b);
    }

    /*----------------------------------------------------------------------*/

    //Cálculo da soma de todos os pixels da imagem resultante

    /*----------------------------------------------------------------------*/
    
    double t1 = agora();

    double soma = 0;

    /*
        Aqui a diretiva pragma tem como seu diferencial a clausula
        reduction(+:soma) que faz com que cada thread tenha sua própria
        variável soma, e no final do laço, todas as somas parciais são
        somadas na variável soma original.
    */

    #pragma omp parallel for reduction(+:soma)

        for (int i = 0; i < N * N; ++i) {
            soma += a[i];
        }

    /*
        Aqui é impresso o resultado final do filtro, 
        que é a soma de todos os pixels da imagem resultante
        e o tempo total de execução do programa.
    */

    printf("N=%d passes=%d  checksum=%.2f  tempo=%.6f s\n", N, P, soma, t1 - t0);
    return 0;
}
