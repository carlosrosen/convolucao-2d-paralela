/* Baseline 04 — Filtro de convolucao 2D (blur 3x3) sobre imagem sintetica
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 * Aspecto interessante: decomposicao de dominio 2D; tratamento de bordas
 * (halo) quando paralelizado em MPI.
 *
 * Compilar: g++ -O2 -fopenmp convolucao_openmp.cpp -o convolucao_openmp
 * Executar: OMP_NUM_THREADS=4 ./convolucao_openmp 2000 30 static
 * Argumentos: lado da imagem, passes e política (static, dynamic ou guided).
 * Imagem: use --input entrada.pgm --output saida.pgm depois dos argumentos.
 */
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <ctime>
#include <string>
#include <omp.h>

using namespace std;

static double agora() {
    timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static string ler_token(ifstream& arquivo) {
    string token;
    while (arquivo >> token) {
        if (!token.empty() && token[0] == '#') {
            arquivo.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }
        return token;
    }
    throw runtime_error("arquivo PGM incompleto");
}

static void carregar_pgm(const string& caminho, vector<float>& imagem, int& N) {
    ifstream arquivo(caminho, ios::binary);
    if (!arquivo) throw runtime_error("nao foi possivel abrir " + caminho);
    if (ler_token(arquivo) != "P5") throw runtime_error("a entrada deve ser PGM binario (P5)");

    int largura = stoi(ler_token(arquivo));
    int altura = stoi(ler_token(arquivo));
    int maximo = stoi(ler_token(arquivo));
    if (largura <= 0 || altura <= 0 || largura != altura)
        throw runtime_error("a imagem PGM deve ser quadrada");
    if (maximo <= 0 || maximo > 255)
        throw runtime_error("o PGM deve usar valores de 8 bits");

    arquivo.get();
    vector<unsigned char> pixels(static_cast<size_t>(largura) * altura);
    arquivo.read(reinterpret_cast<char*>(pixels.data()), pixels.size());
    if (arquivo.gcount() != static_cast<streamsize>(pixels.size()))
        throw runtime_error("dados insuficientes no arquivo PGM");

    N = largura;
    imagem.resize(pixels.size());
    for (size_t i = 0; i < pixels.size(); ++i) imagem[i] = pixels[i];
}

static void salvar_pgm(const string& caminho, const vector<float>& imagem, int N) {
    ofstream arquivo(caminho, ios::binary);
    if (!arquivo) throw runtime_error("nao foi possivel criar " + caminho);
    arquivo << "P5\n" << N << ' ' << N << "\n255\n";
    for (float valor : imagem) {
        if (valor < 0.0f) valor = 0.0f;
        if (valor > 255.0f) valor = 255.0f;
        unsigned char pixel = static_cast<unsigned char>(valor + 0.5f);
        arquivo.write(reinterpret_cast<const char*>(&pixel), 1);
    }
}

int main(int argc, char** argv) {

    int N = (argc > 1) ? atoi(argv[1]) : 2000;
    int P = (argc > 2) ? atoi(argv[2]) : 30;
    string escalonamento = (argc > 3) ? argv[3] : "static";
    bool mostrar_threads = false;
    string entrada;
    string saida;

    for (int i = 4; i < argc; ++i) {
        string argumento = argv[i];
        if (argumento == "--verbose") {
            mostrar_threads = true;
        } else if (argumento == "--input" && i + 1 < argc) {
            entrada = argv[++i];
        } else if (argumento == "--output" && i + 1 < argc) {
            saida = argv[++i];
        } else {
            fprintf(stderr, "Argumento invalido: %s\n", argumento.c_str());
            return 1;
        }
    }

    omp_sched_t politica;
    int tamanho_bloco;

    if (escalonamento == "static") {
        politica = omp_sched_static;
        tamanho_bloco = 0; // divisão estática padrão do runtime
    } else if (escalonamento == "dynamic") {
        politica = omp_sched_dynamic;
        // Blocos evitam uma requisição ao runtime para cada pixel.
        tamanho_bloco = 64;
    } else if (escalonamento == "guided") {
        politica = omp_sched_guided;
        tamanho_bloco = 64;
    } else {
        fprintf(stderr,
                "Escalonamento invalido: %s. Use static, dynamic ou guided.\n",
                escalonamento.c_str());
        return 1;
    }

    // Garante que o runtime respeite a quantidade solicitada em OMP_NUM_THREADS.
    omp_set_dynamic(0);
    omp_set_schedule(politica, tamanho_bloco);

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

        // A impressão por ID é opcional para não poluir os testes de desempenho.
        if (mostrar_threads) {
            for (int vez = 0; vez < total; ++vez) {

                // Sincroniza as threads para manter a impressão em ordem por ID.
                #pragma omp barrier

                if (id == vez) {
                    printf("Thread ativa: %d\n", id);
                }
            }
        }
    }   

    /*----------------------------------------------------------------------*/

    //Inicialização de variáveis e alocação de memória para as imagens

    /*----------------------------------------------------------------------*/

    /*
        a = Imagem de entrada
        b = Imagem que recebe o resultado do filtro
    */
    vector<float> a;

    try {
        if (!entrada.empty()) {
            carregar_pgm(entrada, a, N);
        } else {
            a.resize(static_cast<size_t>(N) * N);
        }
    } catch (const exception& erro) {
        fprintf(stderr, "Erro: %s\n", erro.what());
        return 1;
    }
    vector<float> b = a;


    /*
        Aqui ocorre a paralelização da inicialização da imagem de entrada.
    */
    
    if (entrada.empty()) {
        #pragma omp parallel for schedule(static)

        /*
        Cada pixel recebe um valor entre 0 e 255.
        Isso simula uma imagem em tons de cinza
        sem precisar carregar um arquivo.
        */

        for (int i = 0; i < N * N; ++i) {
            a[i] = static_cast<float>((i * 131) % 256);
        }
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

        /*
            schedule(runtime) permite comparar static, dynamic e guided sem
            recompilar. A política foi escolhida com omp_set_schedule().
        */
        #pragma omp parallel for collapse(2) schedule(runtime)

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

    if (!saida.empty()) {
        try {
            salvar_pgm(saida, a, N);
        } catch (const exception& erro) {
            fprintf(stderr, "Erro: %s\n", erro.what());
            return 1;
        }
    }

    /*
        Aqui é impresso o resultado final do filtro, 
        que é a soma de todos os pixels da imagem resultante
        e o tempo total de execução do programa.
    */

    printf("versao=openmp schedule=%s chunk=%d threads=%d N=%d passes=%d "
           "checksum=%.2f tempo=%.9f s\n",
           escalonamento.c_str(), tamanho_bloco, threads_usadas,
           N, P, soma, t1 - t0);
    return 0;
}
