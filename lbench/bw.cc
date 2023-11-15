#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <iostream>
#include <unistd.h>
#include <chrono>
#include <numa.h>

template<const int NFLOP>
void kernel(size_t nsize, size_t ntrails, double* __restrict__ A)
{
    double alpha = 0.5;

    for (size_t j = 0; j < ntrails; j++) {
        for (size_t i = 0; i < nsize; i++) {
            double beta = 0.8;

            if (NFLOP % 2 == 1)
                beta = A[i] + alpha;

            const int NLOOP = NFLOP/2;
#pragma GCC unroll 16
            for (int k = 0; k < NLOOP; k++)
                beta = beta * A[i] + alpha;

            A[i] = beta;
        }

        alpha = alpha * (1 - 1e-8);
    }
}

static void escape(void *p) {
  asm volatile("" : : "g"(p) : "memory");
}

static void clobber() {
  asm volatile("" : : : "memory");
}

int main(int argc, char **argv)
{
    int init_node, kernel_node;
    if (argc == 4) {
        init_node = -1;
        kernel_node = -1;
    } else if (argc == 6) {
        init_node = atoi(argv[4]);
        kernel_node = atoi(argv[5]);
    } else {
        fprintf(stderr, "usage: bw array_bytes nflops trails [init_node kernel_node]\n");
        exit(1);
    }

    size_t total_size = strtoull(argv[1], NULL, 0);
    int nflop = atoi(argv[2]);
    int trails = atoi(argv[3]);

#pragma omp parallel
    {
        if (init_node >= 0)
            if (numa_run_on_node(init_node) < 0) {
                perror("numa_run_on_node");
                exit(1);
            }

        size_t local_size = total_size / omp_get_num_threads();
        double *A = (double*)malloc(local_size);
        size_t n = local_size / sizeof(double);
        std::chrono::time_point<std::chrono::steady_clock> t0;

        for (size_t i = 0; i < n; i++)
            A[i] = 1.0;

        // load to cache
        kernel<1>(n, 1, A);

        if (kernel_node >= 0)
            if (numa_run_on_node(kernel_node) < 0) {
                perror("numa_run_on_node");
                exit(1);
            }

#pragma omp barrier
#pragma omp master
        {
            std::cout << "setup done. (threads=" << omp_get_num_threads() << " flops=" << nflop << ")" << std::endl;
            t0 = std::chrono::steady_clock::now();
        }

#define CASE(x) case (x): kernel<(x)>(n, trails, A); break
        switch (nflop) {
            default: fprintf(stderr, "error: unsupported nflops %d\n", nflop); exit(1);
            case -1: for (;;) pause(); break;
            CASE(1);
            CASE(2);
            CASE(3);
            CASE(4);
            CASE(5);
            CASE(6);
            CASE(7);
            CASE(8);
            CASE(9);
            CASE(10);
            CASE(11);
            CASE(12);
            CASE(13);
            CASE(14);
            CASE(15);
            CASE(16);
            CASE(17);
            CASE(18);
            CASE(19);
            CASE(20);
            CASE(21);
            CASE(22);
            CASE(23);
            CASE(24);
            CASE(25);
            CASE(26);
            CASE(27);
            CASE(28);
            CASE(29);
            CASE(30);
            CASE(31);
            CASE(32);
            CASE(39);
            CASE(40);
            CASE(48);
            CASE(56);
            CASE(60);
            CASE(64);
            CASE(128);
            CASE(1024);
        }
#undef CASE

        escape(A);
        clobber();

#pragma omp barrier
#pragma omp master
        {
            auto t1 = std::chrono::steady_clock::now();
            std::chrono::duration<double> diff = t1-t0;
            std::cout << "time: " << diff.count() << std::endl;
        }

        free(A);
    }
}
