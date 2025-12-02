#include <mpi.h>
#include <iostream>

int main(int argc, char** argv) {
    const int NUM_SQUIRRELS = 100;

    MPI_Init(&argc, &argv);

    int world_size = 0;
    int world_rank = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    if (world_size != NUM_SQUIRRELS) {
        if (world_rank == 0) {
            std::cerr << "Ошибка: программу нужно запускать с "
                      << NUM_SQUIRRELS << " процессами, а не с "
                      << world_size << std::endl;
        }
        MPI_Finalize();
        return 1;
    }

    MPI_Finalize();
    return 0;
}
