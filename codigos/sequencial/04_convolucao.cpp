/* Baseline sequencial — filtro de convolução 2D (blur 3x3). */
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>

using namespace std;

static double agora() {
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(int argc, char** argv) {
    int N = (argc > 1) ? atoi(argv[1]) : 2000;
    int P = (argc > 2) ? atoi(argv[2]) : 30;
    vector<float> a(N * N), b(N * N);

    for (int i = 0; i < N * N; ++i) {
        a[i] = static_cast<float>((i * 131) % 256);
    }

    double t0 = agora();
    for (int p = 0; p < P; ++p) {
        for (int y = 1; y < N - 1; ++y) {
            for (int x = 1; x < N - 1; ++x) {
                float s = 0;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        s += a[(y + dy) * N + (x + dx)];
                    }
                }
                b[y * N + x] = s / 9.0f;
            }
        }
        a.swap(b);
    }
    double t1 = agora();

    double soma = 0;
    for (int i = 0; i < N * N; ++i) {
        soma += a[i];
    }

    printf("versao=sequencial threads=1 N=%d passes=%d "
           "checksum=%.2f tempo=%.9f s\n",
           N, P, soma, t1 - t0);
    return 0;
}
