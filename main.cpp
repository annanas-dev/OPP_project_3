#include <numeric>
#include <algorithm>
#include <vector>
#include <random>
#include <mpi.h>

int main(int argc, char** argv) {
    const int NUM_SQUIRRELS = 100;
    const int TOTAL_NUTS    = 1000298;

    MPI_Init(&argc, &argv);

    int world_size, world_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    std::vector<double> nuts;
    std::vector<int> sendcounts(world_size);
    std::vector<int> displs(world_size);

    if (world_rank == 0) {
        nuts.resize(TOTAL_NUTS);
        std::mt19937 gen(42);
        std::uniform_real_distribution<double> dist(0.1, 10.0);
        for (int i = 0; i < TOTAL_NUTS; ++i) nuts[i] = dist(gen);

        int base_each = 1;
        long long remaining = TOTAL_NUTS - NUM_SQUIRRELS;
        std::uniform_int_distribution<long long> cut_dist(0, remaining);
        std::vector<long long> cuts(world_size - 1);
        for (auto &c : cuts) c = cut_dist(gen);
        std::sort(cuts.begin(), cuts.end());

        long long prev = 0;
        for (int i = 0; i < world_size - 1; ++i) {
            sendcounts[i] = base_each + static_cast<int>(cuts[i] - prev);
            prev = cuts[i];
        }
        sendcounts[world_size - 1] = base_each + static_cast<int>(remaining - prev);

        displs[0] = 0;
        for (int i = 1; i < world_size; ++i) displs[i] = displs[i-1] + sendcounts[i-1];
    }

    MPI_Finalize();
    return 0;
}
int local_count = 0;
MPI_Scatter(sendcounts.data(), 1, MPI_INT, &local_count, 1, MPI_INT, 0, MPI_COMM_WORLD);

std::vector<double> local_nuts(local_count);
MPI_Scatterv(
    world_rank == 0 ? nuts.data() : nullptr,
    world_rank == 0 ? sendcounts.data() : nullptr,
    world_rank == 0 ? displs.data() : nullptr,
    MPI_DOUBLE,
    local_nuts.data(),
    local_count,
    MPI_DOUBLE,
    0,
    MPI_COMM_WORLD
);
double local_sum = std::accumulate(local_nuts.begin(), local_nuts.end(), 0.0);
double local_avg = (local_count > 0) ? local_sum / local_count : 0.0;

std::vector<double> all_avgs(world_size);
MPI_Allgather(&local_avg, 1, MPI_DOUBLE, all_avgs.data(), 1, MPI_DOUBLE, MPI_COMM_WORLD);

