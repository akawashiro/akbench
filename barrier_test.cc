#include <string>
#include <sys/wait.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <thread>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"

#include "barrier.h"

namespace {

const std::string BRRIER_ID = "/TestBarrier";

void TestConstructor() {
  int pid = fork();
  CHECK(pid >= 0) << "Fork failed: " << strerror(errno);

  if (pid == 0) {
    SenseReversingBarrier barrier(2, BRRIER_ID);
    return;
  } else {
    SenseReversingBarrier barrier(2, BRRIER_ID);
    waitpid(pid, nullptr, 0);
    return;
  }
}

void WaitWithoutSleep(int num_processes, int num_iterations) {
  constexpr double MAX_WAIT_MS = 100.0;
  SenseReversingBarrier barrier(num_processes, BRRIER_ID);

  for (int j = 0; j < num_iterations; ++j) {
    LOG(INFO) << "Waiting at barrier iteration " << j;
    barrier.Wait();
    LOG(INFO) << "Passed barrier iteration " << j;
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
    LOG(INFO) << "Waiting at barrier iteration " << j;
    double sleep_ms = dis(gen);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(static_cast<int>(sleep_ms)));
    barrier.Wait();
    passed_times.push_back(std::chrono::high_resolution_clock::now());
    LOG(INFO) << "Passed barrier iteration " << j;
  }
  return passed_times;
}

void TestWaitWithoutSleep(int num_processes, int num_iterations) {
  std::vector<int> pids;

  for (int i = 0; i < num_processes - 1; ++i) {
    int pid = fork();
    CHECK(pid >= 0) << "Fork failed: " << strerror(errno);

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

  LOG(INFO) << "Wait time: " << duration / num_iterations
            << " ms per iteration.";
  LOG(INFO) << "Wait time: " << duration / num_iterations / num_processes
            << " ms per iteration per process.";
  return;
}

void RecordPassedTimesToFile(
    const std::vector<std::chrono::high_resolution_clock::time_point> &times,
    const std::filesystem::path &file_path) {
  std::ofstream file(file_path);
  if (!file.is_open()) {
    LOG(ERROR) << "Failed to open file for writing: " << file_path;
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
    LOG(ERROR) << "Failed to open file for reading: " << file_path;
    return times;
  }
  std::string line;
  while (std::getline(file, line)) {
    try {
      auto nanoseconds = std::stoll(line);
      times.push_back(std::chrono::high_resolution_clock::time_point(
          std::chrono::nanoseconds(nanoseconds)));
    } catch (const std::exception &e) {
      LOG(ERROR) << "Failed to parse time from file: " << e.what();
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
    LOG(ERROR) << "No passed times to analyze.";
    return;
  }
  const int n_iterations = all_passed_times[0].size();
  const int n_processes = all_passed_times.size();
  for (int i = 0; i < n_processes; i++) {
    CHECK_EQ(all_passed_times[i].size(), n_iterations)
        << "Process " << i << " has a different number of passed times ("
        << all_passed_times[i].size() << ") than the first process ("
        << n_iterations << ").";
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

    LOG(INFO) << "Iteration " << iter << ": "
              << "Average time: " << average_time * 1000 << " ms, "
              << "Standard deviation: " << stddev * 1000 << " ms, "
              << "Max difference: " << max_diff * 1000 << " ms.";
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
    CHECK(pid >= 0) << "Fork failed: " << strerror(errno);

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
    CHECK_EQ(passed_times.size(), read_times.size())
        << "Number of passed times does not match the number of read times.";
    for (size_t i = 0; i < passed_times.size(); ++i) {
      CHECK(passed_times[i] == read_times[i])
          << "Passed time at index " << i
          << " does not match the read time from file.";
    }
    LOG(INFO) << "All passed times match the read times from file.";

    all_passed_times.push_back(passed_times);
  }

  for (int child_pid : pids) {
    waitpid(child_pid, nullptr, 0);
  }

  for (size_t i = 0; i < pids.size(); ++i) {
    const auto read_times = ReadPassedTimesFromFile(
        temp_dir_path / ("process_" + std::to_string(i) + "_times.txt"));
    all_passed_times.push_back(read_times);
    LOG(INFO) << "Read times from file for process " << i << ": "
              << read_times.size() << " entries.";
  }
  AnalizeAllPassedTimes(all_passed_times);
  return;
}

} // namespace

ABSL_FLAG(std::optional<int>, vlog, std::nullopt,
          "Show VLOG messages lower than this level.");
ABSL_FLAG(std::string, test_type, "constructor",
          "Type of test to run. Available types: constructor, "
          "wait_with_random_sleep");
ABSL_FLAG(int, num_processes, 2,
          "Number of processes to use in the wait test.");
ABSL_FLAG(int, num_iterations, 20,
          "Number of iterations to run in the wait test.");

int main(int argc, char *argv[]) {
  absl::SetProgramUsageMessage("Sense Reversing Barrier Test");
  absl::ParseCommandLine(argc, argv);
  const std::optional<int> vlog = absl::GetFlag(FLAGS_vlog);
  if (vlog.has_value()) {
    int v = *vlog;
    absl::SetGlobalVLogLevel(v);
  }
  const std::string test_type = absl::GetFlag(FLAGS_test_type);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  absl::InitializeLog();

  SenseReversingBarrier::ClearResource(BRRIER_ID);

  if (test_type == "constructor") {
    TestConstructor();
  } else if (test_type == "wait_with_random_sleep") {
    int num_processes = absl::GetFlag(FLAGS_num_processes);
    int num_iterations = absl::GetFlag(FLAGS_num_iterations);
    TestWaitWithRandomSleep(num_processes, num_iterations);
  } else if (test_type == "wait_without_sleep") {
    int num_processes = absl::GetFlag(FLAGS_num_processes);
    int num_iterations = absl::GetFlag(FLAGS_num_iterations);
    TestWaitWithoutSleep(num_processes, num_iterations);
  } else {
    LOG(ERROR) << "Unknown test type: " << test_type
               << ". Available types: constructor, wait";
    return 1;
  }

  return 0;
}
