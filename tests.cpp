#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <cmath>

static const int NUM_SQUIRRELS = 100; // кол-во белок
static const long long TOTAL_NUTS = 1000298; // кол-во орехов

// Команда запуска для MPI-программы mpirun c нашим бинарником
// Строковый литерал, который будет началом команды для std::system
static const char* BASE_CMD = "mpirun --oversubscribe -np ";  // далее добавляется число процессов и ./squirrels (далее будет пример)

// Имена файлов, в которые мы будем перенаправлять вывод тестируемой программы
static const char* OUTPUT_FILE_CORRECT = "squirrels_output_correct.txt"; // вывод при правильном числе процессов.
static const char* OUTPUT_FILE_WRONG   = "squirrels_output_wrong.txt"; // при неправильном

// Описывает данные, кот-ые мы парсим из вывода для одной белки
struct SquirrelInfo {
    int id = -1; // номер белки
    long long nuts = 0; // кол-во орехов у этой белки
    double avg = 0.0; // её собственный средний вес орехов
    double left = 0.0; // средний вес соседней белки слева
    double right = 0.0; // средний вес соседней белки справа
};

// Вспомогательная структура, кот-ая управляет запуском тестов
struct TestRunner {
    int passed = 0; 
    int failed = 0;

    // шаблонная функция, тип вызываемого объекта - ф-ия
    template <typename F>
    // метод запускает один тест
    void run(const std::string& name, F func) {
        try {
            func();
            std::cout << "[ OK ] " << name << "\n"; // тест прошел успешно
            ++passed;
        } catch (const std::exception& e) { // ловим исключение
            std::cerr << "[FAIL] " << name << ": " << e.what() << "\n"; // выводим инфо об ошибке
            ++failed;
        }
    }

    // Инфо по тестам
    void summary() const {
        std::cout << "\n=== TEST SUMMARY ===\n";
        std::cout << "Passed: " << passed << "\n";
        std::cout << "Failed: " << failed << "\n";
    }
};

// Макрос, проверяющий некоторое условие cond
// то есть каждый раз, когда в коде встречается ASSERT_TRUE(что-то), 
// нужно подставить вместо этого вот такой текст (тело макроса)
// "\"" стоят, потому что для препроцессора макрос по умолчанию заканчивается на переводе строки
// если условие ложно, то выводится лог об ошибке
#define ASSERT_TRUE(cond)\ 
    do {\
        if (!(cond)) {\ 
            std::ostringstream oss;\ 
            oss << "Assertion failed: " << #cond << " at " << __FILE__ << ":" << __LINE__;\
            throw std::runtime_error(oss.str());\
        }\
    } while (0)

// Макрос, сравнивающий два числа
#define ASSERT_EQ(a, b)\
    do {\
        auto _va = (a);\
        auto _vb = (b);\
        if (!(_va == _vb)) {\
            std::ostringstream oss;\
            oss << "Assertion failed: " << #a << " == " << #b\
                << ", got " << _va << " and " << _vb\
                << " at " << __FILE__ << ":" << __LINE__;\
            throw std::runtime_error(oss.str());\
        }\
    } while (0)


// Макрос, вычисляющий насколько, грубо говоря, два числа равны с некотрой погрешностью 
// насколько два числа близки
#define ASSERT_NEAR(a, b, eps)\
    do {\
        double _va = static_cast<double>(a);\
        double _vb = static_cast<double>(b);\
        if (std::fabs(_va - _vb) > (eps)) {\
            std::ostringstream oss;\
            oss << "Assertion failed: |" << #a << " - " << #b << "| <= " << #eps\
                << ", got " << _va << " and " << _vb\
                << " at " << __FILE__ << ":" << __LINE__;\
            throw std::runtime_error(oss.str());\
        }\
    } while (0)

// Парсим одну строку формата (из таких строк состоит вывод тестируемой программы):
// "Белка r: орехов = X, мой ср. вес = Y, слева = L, справа = R"
bool parse_line(const std::string& line, SquirrelInfo& out) {
    const std::string pref_squirrel = "Белка ";
    const std::string token_nuts    = "орехов = ";
    const std::string token_avg     = "мой ср. вес = ";
    const std::string token_left    = "слева = ";
    const std::string token_right   = "справа = ";

    std::size_t pos = line.find(pref_squirrel);
    if (pos == std::string::npos) return false;

    std::size_t pos_after_id = pos + pref_squirrel.size();
    std::size_t pos_colon = line.find(':', pos_after_id);
    if (pos_colon == std::string::npos) return false;

    // id белки
    out.id = std::stoi(line.substr(pos_after_id, pos_colon - pos_after_id));

    // "орехов = X,"
    std::size_t pos_nuts = line.find(token_nuts, pos_colon);
    if (pos_nuts == std::string::npos) return false;
    pos_nuts += token_nuts.size();
    std::size_t pos_comma1 = line.find(',', pos_nuts);
    if (pos_comma1 == std::string::npos) return false;
    out.nuts = std::stoll(line.substr(pos_nuts, pos_comma1 - pos_nuts));

    // "мой ср. вес = Y,"
    std::size_t pos_avg = line.find(token_avg, pos_comma1);
    if (pos_avg == std::string::npos) return false;
    pos_avg += token_avg.size();
    std::size_t pos_comma2 = line.find(',', pos_avg);
    if (pos_comma2 == std::string::npos) return false;
    out.avg = std::stod(line.substr(pos_avg, pos_comma2 - pos_avg));

    // "слева = L,"
    std::size_t pos_left = line.find(token_left, pos_comma2);
    if (pos_left == std::string::npos) return false;
    pos_left += token_left.size();
    std::size_t pos_comma3 = line.find(',', pos_left);
    if (pos_comma3 == std::string::npos) return false;
    out.left = std::stod(line.substr(pos_left, pos_comma3 - pos_left));

    // "справа = R"
    std::size_t pos_right = line.find(token_right, pos_comma3);
    if (pos_right == std::string::npos) return false;
    pos_right += token_right.size();
    out.right = std::stod(line.substr(pos_right));

    return true;
}

// Запуск программы с заданным числом процессов и выводом в файл
// Возвращает код из std::system
// std::ostringstream cmd; - строковый поток для сборки командной строки
int run_program_with_np(int np, const std::string& out_file) {
    std::ostringstream cmd;
    cmd << BASE_CMD << np << " ./squirrels > " << out_file << " 2>&1"; // запуск тестируемой программы, np - число процессоров
    // пример вида верхней команды: mpirun -np 100 ./squirrels > squirrels_output_correct.txt 2>&1
    return std::system(cmd.str().c_str()); // передаём команду оболочке, запускаем её. Возвращает код возврата процесса
}

// Запускаем программу с правильным числом процессов и читаем её вывод
// squirrels - вектор, куда мы будем складывать распарсенную информацию по всем белкам
// exit_code - сюда положим код возврата запуска
void run_program_and_parse_correct(std::vector<SquirrelInfo>& squirrels, int& exit_code) {
    exit_code = run_program_with_np(NUM_SQUIRRELS, OUTPUT_FILE_CORRECT);

    std::ifstream fin(OUTPUT_FILE_CORRECT);
    if (!fin.is_open()) {
        throw std::runtime_error("Не удалось открыть файл вывода: " + std::string(OUTPUT_FILE_CORRECT));
    }

    std::string line;
    while (std::getline(fin, line)) { // построчно читаем файл
        SquirrelInfo info; // парсим каждую строку
        if (parse_line(line, info)) {
            squirrels.push_back(info);
        }
    }
}

// Ф-ия запуска тестов
int main() {
    std::vector<SquirrelInfo> squirrels; // здесь будут лежать данный обо всех белках, кот-ые мы распарсили из вывода
    int exit_code_correct = 0; // код возврата при запуске с правильным числом процессов

    // Один раз запускаем программу с правильным числом процессов
    // запускаем тестируемую программу один раз, а потом на её выводе крутим все тесты
    try {
        run_program_and_parse_correct(squirrels, exit_code_correct);
    } catch (const std::exception& e) {
        std::cerr << "Ошибка при запуске/парсинге программы: " << e.what() << "\n";
        return 1;
    }

    TestRunner runner;

    // Тест 1: Запуск с правильным числом процессов (код возврата = 0)
    runner.run("Запуск с правильным числом процессов", [&]() {
        ASSERT_EQ(exit_code_correct, 0);
    });

    // Тест 2: Запуск с неправильным числом процессов
    runner.run("Запуск с неправильным числом процессов", [&]() {
        int exit_code_wrong = run_program_with_np(4, OUTPUT_FILE_WRONG);
        ASSERT_TRUE(exit_code_wrong != 0); // при неправильном заупске, код возврата не должен быть равен 0

        // И ожидаем в выводе сообщение об ошибке
        std::ifstream fin(OUTPUT_FILE_WRONG);
        ASSERT_TRUE(fin.is_open());
        // Читаем содержимое файла целиком в строку contents
        // конструктор строки от istreambuf_iterator<char> - стандартный способ прочитать весь файл.
        std::string contents((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
        // Ищем подстроку "Ошибка: программу нужно запускать с" 
        // Если такая строка не находится, тометод возвращает не индекс вхождения подстроки, а специальное значение std::string::npos
        // То есть если у нас ему не равно, т.е оно не возвращено, то сообщение об ошибке найдено и все супер
        ASSERT_TRUE(contents.find("Ошибка: программу нужно запускать с") != std::string::npos);
    });

    // Для удобных проверок построим map id: info
    // ключ - id белки
    // значение - её SquirrelInfo
    // удобно для тестов: можно обратиться к белке по id
    std::map<int, SquirrelInfo> byId;
    for (const auto& s : squirrels) {
        byId[s.id] = s;
    }

    // Тест 3: ровно NUM_SQUIRRELS строк вывода с белками
    runner.run("Ровно 100 строк вывода (по одной на белку)", [&]() {
        ASSERT_EQ(static_cast<int>(squirrels.size()), NUM_SQUIRRELS);
    });

    // Тест 4: id белок от 0 до 99 без пропусков
    // Если какого-то id нет, то тест упадет
    runner.run("Все идентификаторы белок от 0 до 99 присутствуют", [&]() {
        ASSERT_EQ(static_cast<int>(byId.size()), NUM_SQUIRRELS);
        for (int i = 0; i < NUM_SQUIRRELS; ++i) {
            auto it = byId.find(i); // ищем соответ. элемент с ключом i, если находит, возвращает итератор на соответсвующий элемент
            ASSERT_TRUE(it != byId.end()); // если не находит, то возвращает byId.end() - итератор на элемент после последнего
        }
    });

    // Тест 5: сумма орехов по всем белкам равна TOTAL_NUTS
    runner.run("Суммарное число орехов равно TOTAL_NUTS", [&]() {
        long long sum_nuts = 0;
        for (const auto& kv : byId) {
            sum_nuts += kv.second.nuts; // kv.second - значение, kv.first - ключ (id белки)
        }
        ASSERT_EQ(sum_nuts, TOTAL_NUTS);
    });

    // Тест 6: левый сосед согласован со средними
    // Для каждой белки то число, кот-ое она напечатала как для соседки слева должно с определенной точностью
    // совпадать с реальным средним весом соседки слева, , вычисленным по её орехам
    runner.run("Средний вес левого соседа согласован", [&]() {
        const double EPS = 1e-9;
        for (const auto& kv : byId) {
            const auto& s = kv.second;
            int left_id = (s.id - 1 + NUM_SQUIRRELS) % NUM_SQUIRRELS; // ищем id левой соседки нашей белки
            const auto& left_info = byId.at(left_id); // .at() - доступ по ключу
            ASSERT_NEAR(s.left, left_info.avg, EPS);
        }
    });

    // Тест 7: правый сосед согласован со средними 
    runner.run("Средний вес правого соседа согласован", [&]() {
        const double EPS = 1e-9;
        for (const auto& kv : byId) {
            const auto& s = kv.second;
            int right_id = (s.id + 1) % NUM_SQUIRRELS;
            const auto& right_info = byId.at(right_id);
            ASSERT_NEAR(s.right, right_info.avg, EPS);
        }
    });

    // Тест 8: все средние веса неотрицательны
    runner.run("Средние веса неотрицательны", [&]() {
        for (const auto& kv : byId) {
            ASSERT_TRUE(kv.second.avg   >= 0.0);
            ASSERT_TRUE(kv.second.left  >= 0.0);
            ASSERT_TRUE(kv.second.right >= 0.0);
        }
    });

    // Тест 9: есть хотя бы одна белка с ненулевым числом орехов 
    runner.run("Есть хотя бы одна белка с ненулевым числом орехов", [&]() {
        bool found = false;
        for (const auto& kv : byId) {
            if (kv.second.nuts > 0 && kv.second.avg > 0.0) {
                found = true;
                break;
            }
        }
        ASSERT_TRUE(found);
    });

    // Тест 10: Проверка, что ни одна белка не осталась без орехов
    runner.run("Проверка, что ни одна белка не осталась без орехов", [&]() {
        for (const auto& kv : byId) {
            ASSERT_TRUE(kv.second.nuts > 0);
        }
    });

    // Тест 11: для круговой топологии (0 и 99) соседи корректны
    runner.run("Проверка соседей для белок 0 и 99", [&]() {
        const double EPS = 1e-9;

        const auto& s0  = byId.at(0);
        const auto& s99 = byId.at(99);
        const auto& s1  = byId.at(1);

        // Для белки 0: слева 99, справа 1
        ASSERT_NEAR(s0.left,  s99.avg, EPS);
        ASSERT_NEAR(s0.right, s1.avg,  EPS);

        // Для белки 99: слева 98, справа 0
        const auto& s98 = byId.at(98);
        ASSERT_NEAR(s99.left,  s98.avg, EPS);
        ASSERT_NEAR(s99.right, s0.avg,  EPS);
    });

    runner.summary();
    return runner.failed == 0 ? 0 : 1; // возврат кода завершения из main
}
