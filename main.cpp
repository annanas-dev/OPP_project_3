#include <mpi.h>
#include <vector>
#include <random>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <algorithm>

int main(int argc, char** argv) {
    const int NUM_SQUIRRELS = 4;      // количество белок (процессов)
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

    std::vector<double> nuts;                // только на root: все массы
    std::vector<int> sendcounts(world_size); // сколько орехов каждому
    std::vector<int> displs(world_size);     // смещения для Scatterv

    if (world_rank == 0) {
        // Заполнили массы орехов
        nuts.resize(TOTAL_NUTS);
        std::mt19937 gen(42);
        std::uniform_real_distribution<double> dist(0.1, 10.0);
        for (int i = 0; i < TOTAL_NUTS; ++i) nuts[i] = dist(gen);

        // Гарантируем минимум 1 орех каждой белке, остальное распределяем случайно
        if (TOTAL_NUTS < NUM_SQUIRRELS) {
            for (int i = 0; i < world_size; ++i) sendcounts[i] = (i < TOTAL_NUTS ? 1 : 0);
        } else {
            int base_each = 1; // минимум 1
            long long remaining = TOTAL_NUTS - (long long)NUM_SQUIRRELS; 

            std::uniform_int_distribution<long long> cut_dist(0, remaining);
            std::vector<long long> cuts;
            cuts.reserve(world_size - 1);
            for (int i = 0; i < world_size - 1; ++i) cuts.push_back(cut_dist(gen));
            std::sort(cuts.begin(), cuts.end());

            // Разбиваем remaining на части по разрезам
            long long prev = 0;
            for (int r = 0; r < world_size - 1; ++r) {
                long long extra = cuts[r] - prev;
                sendcounts[r] = base_each + static_cast<int>(extra);
                prev = cuts[r];
            }
            // последний кусок
            long long last_extra = remaining - prev;
            sendcounts[world_size - 1] = base_each + static_cast<int>(last_extra);
        }

        // Смещения
        displs[0] = 0;
        for (int r = 1; r < world_size; ++r) displs[r] = displs[r-1] + sendcounts[r-1];

        // Проверка: сумма должна быть TOTAL_NUTS
        long long sum = 0;
        for (int v : sendcounts) sum += v;
        if (sum != TOTAL_NUTS) {
            std::cerr << "Internal error: sum(sendcounts) != TOTAL_NUTS: " << sum << " vs " << TOTAL_NUTS << "\n";
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
    }

    // Каждый процесс получает local_count через Scatter
    int local_count = 0;
    MPI_Scatter(sendcounts.data(), 1, MPI_INT, &local_count, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Принимаем свои орехи
    std::vector<double> local_nuts(local_count);
    MPI_Scatterv(
        world_rank == 0 ? nuts.data() : nullptr,
        world_rank == 0 ? sendcounts.data() : nullptr,
        world_rank == 0 ? displs.data() : nullptr,
        MPI_DOUBLE,
        local_nuts.data(),
        local_count,
        MPI_DOUBLE,
        0, MPI_COMM_WORLD
    );

    // Считаем суммарный и средний вес
    double local_sum = std::accumulate(local_nuts.begin(), local_nuts.end(), 0.0);
    double local_avg = (local_count > 0) ? local_sum / static_cast<double>(local_count) : 0.0;

    // Коллективный обмен: все получают массив средних
    std::vector<double> all_avgs(world_size);
    MPI_Allgather(&local_avg, 1, MPI_DOUBLE, all_avgs.data(), 1, MPI_DOUBLE, MPI_COMM_WORLD);

    // Каждый процесс печатает свою строку
    int left  = (world_rank - 1 + world_size) % world_size;
    int right = (world_rank + 1) % world_size;
    double avg_from_left  = all_avgs[left];
    double avg_from_right = all_avgs[right];

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Белка " << world_rank
              << ": орехов = " << local_count
              << ", мой ср. вес = " << local_avg
              << ", слева = " << avg_from_left
              << ", справа = " << avg_from_right
              << std::endl;

    MPI_Finalize();
    return 0;
}

