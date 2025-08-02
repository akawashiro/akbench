#include <string>
#include <variant>

class MemcpyArguments {
public:
  int data_size;
  bool help;
};

class MultithreadedMemcpyArguments {
public:
  int data_size;
  int n_threads;
  bool help;
};

class LatencyAtomicArguments {
public:
  int loop_size;
  bool help;
};

class AkbenchArguments {
public:
  std::string log_level;
  int n_warmup;
  int n_iterations;
  bool help;
  std::variant<MemcpyArguments, MultithreadedMemcpyArguments,
               LatencyAtomicArguments, std::monostate>
      subcommand_args;
};
