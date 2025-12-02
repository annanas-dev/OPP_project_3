#include <mpi.h>
#include <vector>
#include <random>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <algorithm>

int main(int argc, char** argv) {
    const int NUM_SQUIRRELS = 100;      // количество белок (процессов)
    const int TOTAL_NUTS    = 1000298;  // количество орехов в мешке

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

    std::vector<double> nuts;       
    std::vector<int> sendcounts(world_size);
    std::vector<int> displs(world_size);

    if (world_rank == 0) {
        nuts.resize(TOTAL_NUTS);
        std::mt19937 gen(42);
        std::uniform_real_distribution<double> dist(0.1, 10.0);

        // Случайные массы орехов
        for (int i = 0; i < TOTAL_NUTS; ++i)
            nuts[i] = dist(gen);

        std::vector<int> cuts(world_size - 1);
        std::uniform_int_distribution<int> cut_dist(1, TOTAL_NUTS - 1);

        // 99 случайных "разрезов"
        for (int &c : cuts) c = cut_dist(gen);

        std::sort(cuts.begin(), cuts.end());

        // Генерируем размеры блоков
        int prev = 0;
        for (int r = 0; r < world_size - 1; ++r) {
            sendcounts[r] = cuts[r] - prev;
            prev = cuts[r];
        }
        sendcounts[world_size - 1] = TOTAL_NUTS - prev;

        // Смещения
        displs[0] = 0;
        for (int r = 1; r < world_size; ++r)
            displs[r] = displs[r-1] + sendcounts[r-1];
    }

    int local_count = 0;

    MPI_Scatter(sendcounts.data(), 1, MPI_INT,
                &local_count, 1, MPI_INT,
                0, MPI_COMM_WORLD);

    std::vector<double> local_nuts(local_count);

    MPI_Scatterv(
        world_rank==0 ? nuts.data() : nullptr,
        world_rank==0 ? sendcounts.data() : nullptr,
        world_rank==0 ? displs.data() : nullptr,
        MPI_DOUBLE,
        local_nuts.data(),
        local_count,
        MPI_DOUBLE,
        0, MPI_COMM_WORLD
    );

    double local_sum = std::accumulate(local_nuts.begin(), local_nuts.end(), 0.0);
    double local_avg = (local_count > 0) ? local_sum / local_count : 0;

    std::vector<double> all_avgs(world_size);
    MPI_Allgather(&local_avg, 1, MPI_DOUBLE, all_avgs.data(), 1, MPI_DOUBLE, MPI_COMM_WORLD);

    std::vector<int> all_counts;
    if (world_rank == 0) all_counts.resize(world_size);

    MPI_Gather(&local_count, 1, MPI_INT,
               world_rank==0 ? all_counts.data() : nullptr,
               1, MPI_INT, 0, MPI_COMM_WORLD);

    if (world_rank == 0) {
        std::cout << std::fixed << std::setprecision(4);

        for (int r = 0; r < world_size; ++r) {
            std::cout << "Белка " << r
                      << ": орехов = " << all_counts[r]
                      << ", мой ср. вес = " << all_avgs[r]
                      << ", слева = " << all_avgs[(r-1+world_size)%world_size]
                      << ", справа = " << all_avgs[(r+1)%world_size]
                      << "\n";
        }
    }

    MPI_Finalize();
    return 0;
}
