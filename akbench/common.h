#include <cstdint>
#include <string>
#include <vector>

constexpr uint64_t CHECKSUM_SIZE = 128;
constexpr const char *GIBYTE_PER_SEC_UNIT = " GiByte/sec";

std::vector<uint8_t> GenerateDataToSend(uint64_t data_size);
bool VerifyDataReceived(const std::vector<uint8_t> &data, uint64_t data_size);
double CalculateBandwidth(std::vector<double> durations, int num_iterations,
                          uint64_t data_size);
double CalculateOneTripDuration(const std::vector<double> &durations);
std::string ReceivePrefix(int iteration);
std::string SendPrefix(int iteration);
std::string GenerateUniqueBarrierId(const std::string &base_name);
std::string GenerateUniqueResourcePath(const std::string &base_path);
