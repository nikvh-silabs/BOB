#pragma once

#include "yaml-cpp/yaml.h"
#include "taskflow.hpp"
#include <future>
#include <optional>
#include <filesystem>

#ifdef EXPERIMENTAL_FILESYSTEM
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

namespace yakka {

struct blueprint {
  struct dependency {
    enum dependency_type { DEFAULT_DEPENDENCY, DATA_DEPENDENCY, DEPENDENCY_FILE_DEPENDENCY } type;
    std::string name;
  };
  std::string target;
  std::optional<std::string> regex;
  std::vector<dependency> dependencies; // Unprocessed dependencies. Raw values as found in the YAML.
  nlohmann::json process;
  std::string parent_path;

  blueprint(const std::string &target, const nlohmann::json &blueprint, const std::string &parent_path);
};
} // namespace yakka
