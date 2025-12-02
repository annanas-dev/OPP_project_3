#include <vector>
#include <random>
#include <iostream>
#include <mpi.h>

int main(int argc, char** argv) {
    const int NUM_SQUIRRELS = 100;
    const int TOTAL_NUTS    = 1000298;

    MPI_Init(&argc, &argv);

    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    std::vector<double> nuts;
    if (world_rank == 0) {
        nuts.resize(TOTAL_NUTS);
        std::mt19937 gen(42);
        std::uniform_real_distribution<double> dist(0.1, 10.0);
        for (int i = 0; i < TOTAL_NUTS; ++i) nuts[i] = dist(gen);
    }

    MPI_Finalize();
    return 0;
}
