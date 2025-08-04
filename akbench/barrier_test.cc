#include <getopt.h>
#include <string>
#include <sys/wait.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <thread>
#include <vector>

#include "aklog.h"
#include "barrier.h"
#include "getopt_utils.h"

namespace {

const std::string BRRIER_ID = "/TestBarrier";

void TestConstructor() {
  int pid = fork();
  AKCHECK(pid >= 0, std::format("Fork failed: {}", strerror(errno)));

  if (pid == 0) {
    SenseReversingBarrier barrier(2, BRRIER_ID);
    barrier.Wait();
    return;
  } else {
    SenseReversingBarrier barrier(2, BRRIER_ID);
    barrier.Wait();
    return;
    waitpid(pid, nullptr, 0);
    return;
  }
}

void WaitWithoutSleep(int num_processes, int num_iterations) {
  constexpr double MAX_WAIT_MS = 100.0;
  SenseReversingBarrier barrier(num_processes, BRRIER_ID);

  for (int j = 0; j < num_iterations; ++j) {
    AKLOG(aklog::LogLevel::INFO,
          std::format("Waiting at barrier iteration {}", j));
    barrier.Wait();
    AKLOG(aklog::LogLevel::INFO, std::format("Passed barrier iteration {}", j));
  }
}

std::vector<std::chrono::high_resolution_clock::time_point>
WaitWithRandomSleep(int num_processes, int num_iterations) {
  constexpr double MAX_WAIT_MS = 100.0;
  SenseReversingBarrier barrier(num_processes, BRRIER_ID);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<double> dis(0.0, MAX_WAIT_MS);
  std::vector<std::chrono::high_resolution_clock::time_point> passed_times;

  for (int j = 0; j < num_iterations; ++j) {
    AKLOG(aklog::LogLevel::INFO,
          std::format("Waiting at barrier iteration {}", j));
    double sleep_ms = dis(gen);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(static_cast<int>(sleep_ms)));
    barrier.Wait();
    passed_times.push_back(std::chrono::high_resolution_clock::now());
    AKLOG(aklog::LogLevel::INFO, std::format("Passed barrier iteration {}", j));
  }
  return passed_times;
}

void TestWaitWithoutSleep(int num_processes, int num_iterations) {
  std::vector<int> pids;

  for (int i = 0; i < num_processes - 1; ++i) {
    int pid = fork();
    AKCHECK(pid >= 0, std::format("Fork failed: {}", strerror(errno)));

    if (pid == 0) {
      WaitWithoutSleep(num_processes, num_iterations);
      return;
    } else {
      pids.push_back(pid);
    }
  }

  auto start_time = std::chrono::high_resolution_clock::now();
  WaitWithoutSleep(num_processes, num_iterations);
  auto end_time = std::chrono::high_resolution_clock::now();

  for (int child_pid : pids) {
    waitpid(child_pid, nullptr, 0);
  }
  double duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        end_time - start_time)
                        .count() /
                    10e6;

  AKLOG(aklog::LogLevel::INFO, std::format("Wait time: {} ms per iteration.",
                                           duration / num_iterations));
  AKLOG(aklog::LogLevel::INFO,
        std::format("Wait time: {} ms per iteration per process.",
                    duration / num_iterations / num_processes));
  return;
}

void RecordPassedTimesToFile(
    const std::vector<std::chrono::high_resolution_clock::time_point> &times,
    const std::filesystem::path &file_path) {
  std::ofstream file(file_path);
  if (!file.is_open()) {
    AKLOG(
        aklog::LogLevel::ERROR,
        std::format("Failed to open file for writing: {}", file_path.string()));
    return;
  }

  for (const auto &time : times) {
    auto duration = time.time_since_epoch();
    file << std::chrono::duration_cast<std::chrono::nanoseconds>(duration)
                .count()
         << "\n";
  }
  file.close();
}

std::vector<std::chrono::high_resolution_clock::time_point>
ReadPassedTimesFromFile(const std::filesystem::path &file_path) {
  std::vector<std::chrono::high_resolution_clock::time_point> times;
  std::ifstream file(file_path);
  if (!file.is_open()) {
    AKLOG(
        aklog::LogLevel::ERROR,
        std::format("Failed to open file for reading: {}", file_path.string()));
    return times;
  }
  std::string line;
  while (std::getline(file, line)) {
    try {
      auto nanoseconds = std::stoll(line);
      times.push_back(std::chrono::high_resolution_clock::time_point(
          std::chrono::nanoseconds(nanoseconds)));
    } catch (const std::exception &e) {
      AKLOG(aklog::LogLevel::ERROR,
            std::format("Failed to parse time from file: {}", e.what()));
    }
  }
  file.close();
  return times;
}

void AnalizeAllPassedTimes(
    const std::vector<
        std::vector<std::chrono::high_resolution_clock::time_point>>
        &all_passed_times) {
  if (all_passed_times.empty()) {
    AKLOG(aklog::LogLevel::ERROR, "No passed times to analyze.");
    return;
  }
  const int n_iterations = all_passed_times[0].size();
  const int n_processes = all_passed_times.size();
  for (int i = 0; i < n_processes; i++) {
    AKCHECK(all_passed_times[i].size() == n_iterations,
            std::format("Process {} has a different number of passed times "
                        "({}) than expected ({})",
                        i, all_passed_times[i].size(), n_iterations));
  }

  for (int iter = 0; iter < n_iterations; iter++) {
    std::vector<std::chrono::high_resolution_clock::time_point> times;
    for (int i = 0; i < n_processes; i++) {
      times.push_back(all_passed_times[i][iter]);
    }
    std::sort(times.begin(), times.end());
    std::vector<double> times_duration_secs;
    for (int i = 0; i < n_processes; i++) {
      times_duration_secs.push_back(
          std::chrono::duration_cast<std::chrono::nanoseconds>(times[i] -
                                                               times[0])
              .count() /
          1e9);
    }
    const double average_time =
        std::accumulate(times_duration_secs.begin(), times_duration_secs.end(),
                        0.0) /
        n_processes;
    const double variance =
        std::accumulate(
            times_duration_secs.begin(), times_duration_secs.end(), 0.0,
            [average_time](double acc, double time) {
              return acc + (time - average_time) * (time - average_time);
            }) /
        n_processes;
    const double stddev = std::sqrt(variance);
    const double max_diff = *std::max_element(times_duration_secs.begin(),
                                              times_duration_secs.end()) -
                            *std::min_element(times_duration_secs.begin(),
                                              times_duration_secs.end());

    AKLOG(aklog::LogLevel::INFO,
          std::format("Iteration {}: Average time: {} ms, Standard deviation: "
                      "{} ms, Max difference: {} ms.",
                      iter, average_time * 1000, stddev * 1000,
                      max_diff * 1000));
  }
}

void TestWaitWithRandomSleep(int num_processes, int num_iterations) {
  std::vector<int> pids;

  std::string directory_name =
      "TestWaitWithoutSleep_" +
      std::to_string(
          std::chrono::high_resolution_clock::now().time_since_epoch().count());
  std::filesystem::path temp_dir_path =
      std::filesystem::temp_directory_path() / directory_name;
  std::filesystem::create_directory(temp_dir_path);

  for (int i = 0; i < num_processes - 1; ++i) {
    int pid = fork();
    AKCHECK(pid >= 0, std::format("Fork failed: {}", strerror(errno)));

    if (pid == 0) {
      const auto passed_times =
          WaitWithRandomSleep(num_processes, num_iterations);
      RecordPassedTimesToFile(
          passed_times,
          temp_dir_path / ("process_" + std::to_string(i) + "_times.txt"));
      return;
    } else {
      pids.push_back(pid);
    }
  }

  std::vector<std::vector<std::chrono::high_resolution_clock::time_point>>
      all_passed_times;
  {
    const auto passed_times =
        WaitWithRandomSleep(num_processes, num_iterations);
    RecordPassedTimesToFile(passed_times,
                            temp_dir_path / "main_process_times.txt");
    const auto read_times =
        ReadPassedTimesFromFile(temp_dir_path / "main_process_times.txt");
    AKCHECK(passed_times.size() == read_times.size(),
            "Number of passed times does not match the number of read times.");
    for (size_t i = 0; i < passed_times.size(); ++i) {
      AKCHECK(
          passed_times[i] == read_times[i],
          std::format(
              "Passed time at index {} does not match the read time from file.",
              i));
    }
    AKLOG(aklog::LogLevel::INFO,
          "All passed times match the read times from file.");

    all_passed_times.push_back(passed_times);
  }

  for (int child_pid : pids) {
    waitpid(child_pid, nullptr, 0);
  }

  for (size_t i = 0; i < pids.size(); ++i) {
    const auto read_times = ReadPassedTimesFromFile(
        temp_dir_path / ("process_" + std::to_string(i) + "_times.txt"));
    all_passed_times.push_back(read_times);
    AKLOG(aklog::LogLevel::INFO,
          std::format("Read times from file for process {}: {} entries.", i,
                      read_times.size()));
  }
  AnalizeAllPassedTimes(all_passed_times);
  return;
}

} // namespace

// Command line option variables
static std::string g_test_type = "constructor";
static int g_num_processes = 2;
static int g_num_iterations = 20;

void PrintUsage(const char *program_name) {
  std::cout << R"(Usage: )" << program_name << R"( [OPTIONS]

Sense Reversing Barrier Test

Options:
  -t, --test-type=TYPE     Type of test to run (default: constructor)
                           Available types: constructor,
                           wait_with_random_sleep, wait_without_sleep
  -p, --num-processes=N    Number of processes for wait test (default: 2)
  -i, --num-iterations=N   Number of iterations for wait test (default: 20)
  -h, --help               Display this help message
)";
}

int main(int argc, char *argv[]) {
  const char *program_name = argv[0];

  // Define long options
  static struct option long_options[] = {
      {"test-type", required_argument, nullptr, 't'},
      {"num-processes", required_argument, nullptr, 'p'},
      {"num-iterations", required_argument, nullptr, 'i'},
      {"help", no_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};

  // Parse command line options
  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "t:p:i:h", long_options,
                            &option_index)) != -1) {
    try {
      switch (opt) {
      case 't':
        g_test_type = optarg;
        break;
      case 'p':
        g_num_processes = ParseInt(optarg);
        break;
      case 'i':
        g_num_iterations = ParseInt(optarg);
        break;
      case 'h':
        PrintUsage(program_name);
        return 0;
      case '?':
        // getopt_long already printed an error message
        return 1;
      default:
        PrintErrorAndExit(program_name, "Unknown option");
      }
    } catch (const std::exception &e) {
      PrintErrorAndExit(program_name, e.what());
    }
  }

  // Check for extra arguments
  if (optind < argc) {
    PrintErrorAndExit(program_name,
                      "Unexpected argument: " + std::string(argv[optind]));
  }

  const std::string &test_type = g_test_type;

  SenseReversingBarrier::ClearResource(BRRIER_ID);

  if (test_type == "constructor") {
    TestConstructor();
  } else if (test_type == "wait_with_random_sleep") {
    int num_processes = g_num_processes;
    int num_iterations = g_num_iterations;
    TestWaitWithRandomSleep(num_processes, num_iterations);
  } else if (test_type == "wait_without_sleep") {
    int num_processes = g_num_processes;
    int num_iterations = g_num_iterations;
    TestWaitWithoutSleep(num_processes, num_iterations);
  } else {
    AKLOG(aklog::LogLevel::ERROR,
          std::format("Unknown test type: {}. Available types: constructor, "
                      "wait_with_random_sleep, wait_without_sleep",
                      test_type));
    return 1;
  }

  return 0;
}
