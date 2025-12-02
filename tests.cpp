#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <cmath>

// ===============================================
// Настройка количества процессов и суммарных орехов
// ===============================================
static const int NUM_SQUIRRELS = 4; 
static const long long TOTAL_NUTS = 1000298;

// Команда для запуска MPI
static const char* BASE_CMD = "mpirun -np ";

// Файлы для вывода программы
static const char* OUTPUT_FILE_CORRECT = "squirrels_output_correct.txt";
static const char* OUTPUT_FILE_WRONG   = "squirrels_output_wrong.txt";

// -----------------------------------------------
// Структура для хранения одной строки вывода белки
// -----------------------------------------------
struct SquirrelInfo {
    int id = -1;
    long long nuts = 0;
    double avg = 0.0;
    double left = 0.0;
    double right = 0.0;
};

// -----------------------------------------------
// Мини-фреймворк для тестов (подсчёт passed/failed)
// -----------------------------------------------
struct TestRunner {
    int passed = 0;
    int failed = 0;

    template <typename F>
    void run(const std::string& name, F func) {
        try {
            func();
            std::cout << "[ OK ] " << name << "\n";
            ++passed;
        } catch (const std::exception& e) {
            std::cerr << "[FAIL] " << name << ": " << e.what() << "\n";
            ++failed;
        }
    }

    void summary() const {
        std::cout << "\n=== TEST SUMMARY ===\n";
        std::cout << "Passed: " << passed << "\n";
        std::cout << "Failed: " << failed << "\n";
    }
};

// Макросы для проверки
#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::ostringstream oss; \
            oss << "Assertion failed: " << #cond; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        if (!((a) == (b))) { \
            std::ostringstream oss; \
            oss << "Assertion failed: " << #a << " == " << #b; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_NEAR(a, b, eps) \
    do { \
        if (std::fabs((a) - (b)) > (eps)) { \
            std::ostringstream oss; \
            oss << "Assertion failed: " << #a << " ~ " << #b; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

// -----------------------------------------------------------
// Функция парсит одну строку формата
// "Белка X: орехов = A, мой ср. вес = B, слева = C, справа = D"
// -----------------------------------------------------------
bool parse_line(const std::string& line, SquirrelInfo& out) {
    const std::string pref_squirrel = "Белка ";
    const std::string token_nuts    = "орехов = ";
    const std::string token_avg     = "мой ср. вес = ";
    const std::string token_left    = "слева = ";
    const std::string token_right   = "справа = ";

    // ищем "Белка X:"
    std::size_t pos = line.find(pref_squirrel);
    if (pos == std::string::npos) return false;
    std::size_t pos_after_id = pos + pref_squirrel.size();
    std::size_t pos_colon = line.find(':', pos_after_id);
    if (pos_colon == std::string::npos) return false;
    out.id = std::stoi(line.substr(pos_after_id, pos_colon - pos_after_id));

    // орехи
    std::size_t pos_nuts = line.find(token_nuts, pos_colon);
    if (pos_nuts == std::string::npos) return false;
    pos_nuts += token_nuts.size();
    std::size_t pos_comma1 = line.find(',', pos_nuts);
    if (pos_comma1 == std::string::npos) return false;
    out.nuts = std::stoll(line.substr(pos_nuts, pos_comma1 - pos_nuts));

    // средний вес
    std::size_t pos_avg = line.find(token_avg, pos_comma1);
    if (pos_avg == std::string::npos) return false;
    pos_avg += token_avg.size();
    std::size_t pos_comma2 = line.find(',', pos_avg);
    if (pos_comma2 == std::string::npos) return false;
    out.avg = std::stod(line.substr(pos_avg, pos_comma2 - pos_avg));

    // слева
    std::size_t pos_left = line.find(token_left, pos_comma2);
    if (pos_left == std::string::npos) return false;
    pos_left += token_left.size();
    std::size_t pos_comma3 = line.find(',', pos_left);
    if (pos_comma3 == std::string::npos) return false;
    out.left = std::stod(line.substr(pos_left, pos_comma3 - pos_left));

    // справа
    std::size_t pos_right = line.find(token_right, pos_comma3);
    if (pos_right == std::string::npos) return false;
    pos_right += token_right.size();
    out.right = std::stod(line.substr(pos_right));

    return true;
}

// -----------------------------------------------------------
// Запуск программы с указанным -np и запись в файл
// -----------------------------------------------------------
int run_program_with_np(int np, const std::string& out_file) {
    std::ostringstream cmd;
    cmd << BASE_CMD << np << " ./squirrels > " << out_file << " 2>&1";
    return std::system(cmd.str().c_str());
}

// -----------------------------------------------------------
// Запуск белок с правильным -np (4) и парсинг вывода
// -----------------------------------------------------------
void run_program_and_parse_correct(std::vector<SquirrelInfo>& squirrels, int& exit_code) {
    exit_code = run_program_with_np(NUM_SQUIRRELS, OUTPUT_FILE_CORRECT);

    std::ifstream fin(OUTPUT_FILE_CORRECT);
    if (!fin.is_open()) {
        throw std::runtime_error("Не удалось открыть файл вывода");
    }

    // построчно читаем и парсим
    std::string line;
    while (std::getline(fin, line)) {
        SquirrelInfo info;
        if (parse_line(line, info)) {
            squirrels.push_back(info);
        }
    }
}

// ===========================================================
//                    MAIN — ЗАПУСК ТЕСТОВ
// ===========================================================
int main() {
    std::vector<SquirrelInfo> squirrels;
    int exit_code_correct = 0;

    // 1. Запускаем программу и парсим результат
    try {
        run_program_and_parse_correct(squirrels, exit_code_correct);
    } catch (const std::exception& e) {
        std::cerr << "Ошибка запуска/парсинга: " << e.what() << "\n";
        return 1;
    }

    TestRunner runner;

    // ----------------------------------------------------
    // ТЕСТ 1 — правильный запуск с -np 4 должен возвращать 0
    // ----------------------------------------------------
    runner.run("Запуск с правильным числом процессов", [&]() {
        ASSERT_EQ(exit_code_correct, 0);
    });

    // ----------------------------------------------------
    // ТЕСТ 2 — запуск с неправильным числом процессов (3)
    // должен вернуть ошибку и слово "Ошибка" в выводе
    // ----------------------------------------------------
    runner.run("Запуск с неправильным числом процессов", [&]() {
        int exit_code_wrong = run_program_with_np(3, OUTPUT_FILE_WRONG);
        ASSERT_TRUE(exit_code_wrong != 0);

        std::ifstream fin(OUTPUT_FILE_WRONG);
        ASSERT_TRUE(fin.is_open());
        std::string contents((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
        ASSERT_TRUE(contents.find("Ошибка") != std::string::npos);
    });

    // преобразуем белок в map по ID
    std::map<int, SquirrelInfo> byId;
    for (const auto& s : squirrels) byId[s.id] = s;

    // ----------------------------------------------------
    // ТЕСТ 3 — должно быть ровно 4 строки вывода
    // ----------------------------------------------------
    runner.run("Ровно 4 строки вывода", [&]() {
        ASSERT_EQ((int)squirrels.size(), NUM_SQUIRRELS);
    });

    // ----------------------------------------------------
    // ТЕСТ 4 — белки с ID 0,1,2,3 должны существовать
    // ----------------------------------------------------
    runner.run("ID белок 0–3 присутствуют", [&]() {
        ASSERT_EQ((int)byId.size(), NUM_SQUIRRELS);
        for (int i = 0; i < NUM_SQUIRRELS; i++)
            ASSERT_TRUE(byId.count(i));
    });

    // ----------------------------------------------------
    // ТЕСТ 5 — сумма орехов всех белок = TOTAL_NUTS
    // ----------------------------------------------------
    runner.run("Суммарное число орехов равно TOTAL_NUTS", [&]() {
        long long sum = 0;
        for (auto& kv : byId) sum += kv.second.nuts;
        ASSERT_EQ(sum, TOTAL_NUTS);
    });

    // ----------------------------------------------------
    // ТЕСТ 6 — проверка правильности передачи среднего веса слева
    // ----------------------------------------------------
    runner.run("Левые соседи корректны", [&]() {
        for (auto& kv : byId) {
            int left = (kv.first - 1 + NUM_SQUIRRELS) % NUM_SQUIRRELS;
            ASSERT_NEAR(kv.second.left, byId[left].avg, 1e-9);
        }
    });

    // ----------------------------------------------------
    // ТЕСТ 7 — проверка правильности передачи среднего веса справа
    // ----------------------------------------------------
    runner.run("Правые соседи корректны", [&]() {
        for (auto& kv : byId) {
            int right = (kv.first + 1) % NUM_SQUIRRELS;
            ASSERT_NEAR(kv.second.right, byId[right].avg, 1e-9);
        }
    });

    // ----------------------------------------------------
    // ТЕСТ 8 — средние значения не бывают отрицательными
    // ----------------------------------------------------
    runner.run("Средние веса неотрицательны", [&]() {
        for (auto& kv : byId) {
            ASSERT_TRUE(kv.second.avg >= 0);
            ASSERT_TRUE(kv.second.left >= 0);
            ASSERT_TRUE(kv.second.right >= 0);
        }
    });

    // ----------------------------------------------------
    // ТЕСТ 9 — у хотя бы одной белки орехи > 0
    // ----------------------------------------------------
    runner.run("Хотя бы одна белка имеет ненулевые орехи", [&]() {
        bool ok = false;
        for (auto& kv : byId)
            if (kv.second.nuts > 0) ok = true;
        ASSERT_TRUE(ok);
    });

    // ----------------------------------------------------
    // ТЕСТ 10 — ни одна белка не пустая
    // ----------------------------------------------------
    runner.run("Ни одна белка не осталась без орехов", [&]() {
        for (auto& kv : byId)
            ASSERT_TRUE(kv.second.nuts > 0);
    });

    // Вывод итогов
    runner.summary();
    return runner.failed == 0 ? 0 : 1;
}
