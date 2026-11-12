/* Baseline 04 — Filtro de convolucao 2D (blur 3x3) sobre imagem sintetica
 * IESB 2026/2 — CCO085 — Prof. Rodrigo Goncalves Pinto
 * Aspecto interessante: decomposicao de dominio 2D; tratamento de bordas
 * (halo) quando paralelizado em MPI.
 * 
 * Código modificado e analisado por: Carlos Eduardo Roseno Paiva
 *
 * Compilar: g++ -O2 -o perfilamento perfilamento.cpp
 * Executar: ./perfilamento 2000 30   (lado da imagem; num. de passes)
 */

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <ctime>
using namespace std;

static double agora() {
    timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(int argc, char** argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 2000;
    int P = (argc > 2) ? atoi(argv[2]) : 30;

    // inicio da medicao do tempo do algoritmo
    double t_total0 = agora();

    // tempo de alocacao e zeragem dos dois buffers
    double t_aloc0 = agora();
    vector<float> a(N*N), b(N*N);
    double t_aloc1 = agora();

    // tempo da inicializacao de a
    double t_init0 = agora();
    for (int i = 0; i < N*N; i++) a[i] = (float)((i * 131) % 256);
    double t_init1 = agora();

    // tempo da convolucao
    double t_conv0 = agora();
    for (int p = 0; p < P; p++) {
        for (int y = 1; y < N-1; y++)
            for (int x = 1; x < N-1; x++) {
                float s = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        s += a[(y+dy)*N + (x+dx)];
                b[y*N + x] = s / 9.0f;
            }
        a.swap(b);
    }
    double t_conv1 = agora();

    // tempo do checksum
    double t_check0 = agora();
    double soma = 0;
    for (int i = 0; i < N*N; i++) soma += a[i];
    double t_check1 = agora();

    // fim da medicao do tempo do algoritmo
    double t_total1 = agora();

    // tempos de cada bloco
    double t_aloc  = t_aloc1  - t_aloc0;
    double t_init  = t_init1  - t_init0;
    double t_conv  = t_conv1  - t_conv0;
    double t_check = t_check1 - t_check0;
    double t_total = t_total1 - t_total0;

    // diferenca entre o total e a soma dos blocos
    // chamadas de agora() e pequenas folgas entre os blocos
    double outros = t_total - (t_aloc + t_init + t_conv + t_check);

    // Percentual de cada bloco em relacao ao total.
    double p_aloc  = (t_total > 0) ? 100.0 * t_aloc  / t_total : 0.0;
    double p_init  = (t_total > 0) ? 100.0 * t_init  / t_total : 0.0;
    double p_conv  = (t_total > 0) ? 100.0 * t_conv  / t_total : 0.0;
    double p_check = (t_total > 0) ? 100.0 * t_check / t_total : 0.0;
    double p_outros = (t_total > 0) ? 100.0 * outros  / t_total : 0.0;

    // saida no mesmo formato do baseline, para comparacao direta
    printf("N=%d passes=%d  checksum=%.2f  tempo=%.6f s", N, P, soma, t_conv);

    // identifica o bloco que concentra o maior tempo
    int hotspot = 0;  // 0=alocacao, 1=init, 2=convolucao, 3=checksum
    double t_hotspot = t_aloc;
    double p_hotspot = p_aloc;
    const char* nome_hotspot = "Alocacao";

    if (t_init > t_hotspot) {
        hotspot = 1; 
        t_hotspot = t_init; 
        p_hotspot = p_init; 
        nome_hotspot = "Inicializacao";
    }
    if (t_conv > t_hotspot) {
        hotspot = 2; 
        t_hotspot = t_conv; 
        p_hotspot = p_conv; 
        nome_hotspot = "Convolucao";
    }
    if (t_check > t_hotspot) {
        hotspot = 3; 
        t_hotspot = t_check; 
        p_hotspot = p_check; 
        nome_hotspot = "Checksum";
    }

    // tabela de perfilamento, com o hotspot marcado
    printf("\nTabela de Perfilamento:\n");
    printf("%-22s %14s %10s\n", "Bloco", "Tempo (s)", "%");
    printf("%-22s %14.9f %9.2f%%%s\n", "Alocacao", t_aloc,  p_aloc,  hotspot == 0 ? "  <-- hotspot" : "");
    printf("%-22s %14.9f %9.2f%%%s\n", "Inicializacao", t_init,  p_init,  hotspot == 1 ? "  <-- hotspot" : "");
    printf("%-22s %14.9f %9.2f%%%s\n", "Convolucao", t_conv,  p_conv,  hotspot == 2 ? "  <-- hotspot" : "");
    printf("%-22s %14.9f %9.2f%%%s\n", "Checksum", t_check, p_check, hotspot == 3 ? "  <-- hotspot" : "");
    printf("%-22s %14.9f %9.2f%%\n", "Outros", outros,  p_outros);
    printf("%-22s %14.9f %9.2f%%\n", "Total", t_total, 100.0);
    printf("\nHotspot: %s (%.9f s, %.2f%% do total)\n", nome_hotspot, t_hotspot, p_hotspot);
    printf("Soma dos blocos = %.9f s | Total = %.9f s | Diferenca = %.9f s\n",t_aloc + t_init + t_conv + t_check, t_total, outros);

    return 0;
}
