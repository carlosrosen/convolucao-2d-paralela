/* Baseline 04 — Filtro de convolucao 2D (blur 3x3) sobre imagem sintetica
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 * Aspecto interessante: decomposicao de dominio 2D; tratamento de bordas
 * (halo) quando paralelizado em MPI.
 *
 * Compilar: g++ -O2 -o 04_convolucao 04_convolucao.cpp
 * Executar: ./04_convolucao 2000 30   (lado da imagem; num. de passes)
 */

/*
    Modificado por Davi Roberto Barros Ferreira
    para a implementação do MPI (Message Passing Interface)
    versão 1.0
*/

#include <cstdlib>
#include <iostream>
#include <vector>
#include <ctime>

#include <mpi.h>
#include <cstring>
#include <unistd.h>
using namespace std;

#define END_MSG 0 // TAG indicating no buffer will come more
#define PIPE_MSG 1 // TAG indicating buffer came from orquestrator

double T1, T2; //cria as variaveis para medir o tempo
int world_size, world_rank; //cria as variaveis para identificar quantos nodes tem e qual é o node atual

int N, PASSES; // declara as variaveis de paramêntros como globais para facilitar o acesso em cada node.



typedef struct PacoteConvolucao{
    int num_linha; // para o node 0 saber aonde colocar os dados
    vector<float> dados;
}PacoteConvolucao;

typedef struct PacoteFonte{
    int num_linha_meio; // para passar a referencia da linha para quando for retornar ter o indice para onde colocar os dados
    vector<float> linha_superior, linha_meio, linha_inferior;
}PacoteFonte;


double nodeSerial(vector<float> &dados, vector<float> &saida);
double node0(vector<float> &dados, vector<float> &saida);
void nodeN();
float convolucao(vector<float> l_superior, vector<float> l_meio, vector<float> l_inferior, int col);
void exibir_metricas_finais(int N, int passes,vector<float> &matriz_resultado, float tempo);

vector<byte> serializar_fonte(int num_linha_meio, vector<float> &dados);
PacoteFonte deserializar_fonte(vector<byte> buffer);
vector<byte> serializar_resultado_convolucao(PacoteConvolucao p);
PacoteConvolucao deserializar_resultado_convolucao(vector<byte> buffer);



int main(int argc, char** argv) {
    // inicializa os dados antes de dividir em varios processos
    N = (argc > 1) ? atoi(argv[1]) : 2000;
    PASSES = (argc > 2) ? atoi(argv[2]) : 30;
    vector<float> dados(N*N),saida(N*N);
    
    // Começa a inicialização da MPI
    MPI_Init(&argc, &argv); //Inicializa a interface de passagem de mensagem (cria os novos processos)
    MPI_Comm_size(MPI_COMM_WORLD, &world_size); // atribui a variavel world_size quantos nodes foram criados
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank); // atribui a variavel world_rank qual node esse processo é    


    if(world_size == 1){ // roda o algoritmo sequencialmente se so tiver uma maquina.
        for (int i = 0; i < N*N; i++) dados[i] = (float)((i * 131) % 256);
        
        double tempo = nodeSerial(dados,saida);
        exibir_metricas_finais(N,PASSES,saida,tempo);
    }else if(world_rank == 0){ //separa a maquina 0 para orquestrar as mensagens
        for (int i = 0; i < N*N; i++) dados[i] = (float)((i * 131) % 256);

        double tempo = node0(dados,saida);
        exibir_metricas_finais(N,PASSES,dados,tempo);
    }else{
        nodeN();
    }
    MPI_Finalize();
    return 0;
}

// orquestrador, ele seleciona o que os outros nodes vão processar e depois reune tudo na matriz final
double node0(vector<float> &dados, vector<float> &saida){
    int Error, dummy;
    MPI_Status status;

    const size_t buffer_send_size = sizeof(int) + (N*sizeof(float)*3);
    const size_t buffer_receive_size = sizeof(int) + (sizeof(float) * N);
    vector<byte> buffer_send_data(buffer_send_size), buffer_receive_data(buffer_receive_size);
    
    double T0 = MPI_Wtime();
    for (int p = 0; p < PASSES; p++) {
        for(int row = 1; row < N-1; row++){
            
            size_t line_size = sizeof(float) * N; // calcula o tamanho de uma linha da matriz original
            
            buffer_send_data = serializar_fonte(row, dados);

            int dest = (row % (world_size - 1)) + 1;
            
            Error = MPI_Send(buffer_send_data.data(),buffer_send_size,MPI_BYTE,dest,PIPE_MSG,MPI_COMM_WORLD);
        }
        int count_receive = 0;
        while(1){
            Error = MPI_Recv(buffer_receive_data.data(),buffer_receive_size,MPI_BYTE,MPI_ANY_SOURCE,PIPE_MSG,MPI_COMM_WORLD,&status);

            PacoteConvolucao p_conv = deserializar_resultado_convolucao(buffer_receive_data);
                        

            float first, last; // para salvar o primeiro e último valor da linha
            first = saida[p_conv.num_linha*N];
            last = saida[p_conv.num_linha*N + N -1];
            
            memcpy(saida.data() + (p_conv.num_linha*N),p_conv.dados.data(), sizeof(float)* N);
            
            saida[p_conv.num_linha*N] = first;
            saida[p_conv.num_linha*N + N -1] = last;
            
            count_receive++;
            if(count_receive == N -2 )break;
        }

        dados.swap(saida);
    }
    for(int i = 1; i < world_size; i++){
        Error = MPI_Send(&dummy,1,MPI_INT,i,END_MSG,MPI_COMM_WORLD); // Manda encerrar os outros nodes.
    }
    return (MPI_Wtime() - T0);
}

void nodeN(){    
    int Error;
    MPI_Status status;
    vector<float> conv(N);
    const size_t buffer_receive_size = sizeof(int) + (N*sizeof(float)*3);
    const size_t buffer_send_size = sizeof(int) + (N * sizeof(float));
    vector<byte> buffer_send_data(buffer_send_size), buffer_receive_data(buffer_receive_size);
    
    while(1){
        Error = MPI_Recv(buffer_receive_data.data(),buffer_receive_size,MPI_BYTE,0,MPI_ANY_TAG,MPI_COMM_WORLD,&status);

        if(status.MPI_TAG == END_MSG){
            break;
        }

        PacoteFonte fonte = deserializar_fonte(buffer_receive_data);

        for(int col = 1; col < N-1; col++){
            conv[col] = convolucao(fonte.linha_superior, fonte.linha_meio, fonte.linha_inferior, col);
        }

        PacoteConvolucao p_conv = {fonte.num_linha_meio, conv};
        buffer_send_data = serializar_resultado_convolucao(p_conv);

        MPI_Send(buffer_send_data.data(),sizeof(int) + sizeof(float)*N,MPI_BYTE,0,PIPE_MSG,MPI_COMM_WORLD);
    }
}

double nodeSerial(vector<float> &dados, vector<float> &saida){
    float T0 = (float)MPI_Wtime();
    for (int pack_resultado = 0; pack_resultado < PASSES; pack_resultado++) {
        for (int y = 1; y < N-1; y++)
        for (int x = 1; x < N-1; x++) {
            float s = 0;
            for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
            s += dados[(y+dy)*N + (x+dx)];
            saida[y*N + x] = s / 9.0f;
        }
        dados.swap(saida);
    }
    return MPI_Wtime() - T0;
}

// gera o valor resultante da operação de convolução 
float convolucao(vector<float> l_superior, vector<float> l_meio, vector<float> l_inferior, int col){
    float soma = 0;
    vector<float> kernel = {1.0f/9.0f,1.0f/9.0f,1.0f/9.0f,  1.0f/9.0f,1.0f/9.0f,1.0f/9.0f,  1.0f/9.0f,1.0f/9.0f,1.0f/9.0f};
    for(int idx_kernel = -1; idx_kernel < 2; idx_kernel++){
        soma += (
            (l_superior[idx_kernel+ col] * kernel[idx_kernel + 1]) +
            (l_meio[idx_kernel +col] * kernel[(idx_kernel + 1) + 3]) +
            (l_inferior[idx_kernel + col] * kernel[(idx_kernel + 1) + 6])
        );
    }
    return soma;
}

void exibir_metricas_finais(int N, int passes,vector<float> &matriz_resultado, float tempo){
    float soma = 0;
    for (int i = 0; i < N*N; i++) {
        soma += matriz_resultado[i];
    }
    printf("N=%d passes=%d  checksum=%.2f  tempo=%.6f s\n", N, PASSES, soma, tempo);
}


/*
    cria um vetor de bytes com o a capacidade para 
    respectivamente portar 1 inteiro (o numero da linha da matriz original)
    e 3 arrays de float (o 1° array é a linha superior, 2° é a linha do meio e o 3° é a linha inferior)
*/ 
vector<byte> serializar_fonte(int num_linha_meio, vector<float> &dados){
    vector<byte> buffer(sizeof(int) + (3 * sizeof(float) * N));
    memcpy(buffer.data(),&num_linha_meio,sizeof(int));
    
    // percorre o tamanho de uma linha e copia todos os valores necessarios para cada linha do buffer
    memcpy(buffer.data() + sizeof(int), dados.data() + (num_linha_meio * N - N) , sizeof(float) * N);
    memcpy(buffer.data() + sizeof(int) + (sizeof(float) * N), dados.data() + (num_linha_meio * N) , sizeof(float) * N);
    memcpy(buffer.data() + sizeof(int) + (2 * sizeof(float) * N), dados.data() + (num_linha_meio * N + N) , sizeof(float) * N);
    
    return buffer;
}

PacoteFonte deserializar_fonte(vector<byte> buffer){
    PacoteFonte p = {-1, vector<float> (N),vector<float> (N),vector<float> (N)};
    
    memcpy(&p.num_linha_meio, buffer.data(), sizeof(int));
    // vector<float> l_superior(N), l_meio(N), l_inferior(N);
    
    memcpy(p.linha_superior.data(),buffer.data() + sizeof(int), sizeof(float)*N);
    memcpy(p.linha_meio.data(), buffer.data() + sizeof(int) + (sizeof(float) * N), sizeof(float)*N);
    memcpy(p.linha_inferior.data(), buffer.data() + sizeof(int) + (2 * sizeof(float) * N), sizeof(float)*N);
    
    return p;
}

vector<byte> serializar_resultado_convolucao(PacoteConvolucao p){
    vector<byte> buffer(sizeof(int) + (N * sizeof(float)));
    memcpy(buffer.data(), &p.num_linha, sizeof(int));
        
    memcpy(buffer.data() + sizeof(int),p.dados.data(),sizeof(float) * N);
    return buffer;
}

PacoteConvolucao deserializar_resultado_convolucao(vector<byte> buffer){
    PacoteConvolucao p = {-1, vector<float> (N)};

    memcpy(&p.num_linha,buffer.data(), sizeof(int));
    memcpy(p.dados.data(),buffer.data() + sizeof(int), sizeof(float)* N);

    return p;
}