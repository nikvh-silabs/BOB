#pragma once

#include "utilities.hpp"
#include "yaml-cpp/yaml.h"
#include <set>
#include <string>
#include <functional>
#include <unordered_set>
#include <future>

namespace yakka {
const std::string yakka_component_extension     = ".yakka";
const std::string yakka_component_old_extension = ".bob";
const char data_dependency_identifier           = ':';
const char data_wildcard_identifier             = '*';
const std::string database_filename             = "yakka-components.yaml";

#if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
const std::string host_os_string         = "windows";
const std::string executable_extension   = ".exe";
const std::string host_os_path_seperator = ";";
const auto async_launch_option           = std::launch::async | std::launch::deferred;
#elif defined(__APPLE__)
const std::string host_os_string         = "macos";
const std::string executable_extension   = "";
const std::string host_os_path_seperator = ":";
const auto async_launch_option           = std::launch::async | std::launch::deferred; // Unsure
#elif defined(__linux__)
const std::string host_os_string         = "linux";
const std::string executable_extension   = "";
const std::string host_os_path_seperator = ":";
const auto async_launch_option           = std::launch::deferred;
#endif

struct process_return {
  std::string result;
  int retcode;
};

struct project_description {
  std::vector<std::string> components;
  std::vector<std::string> features;
};

enum yakka_status {
  SUCCESS = 0,
  FAIL,
};

void fetch_component(const std::string &name, YAML::Node node, std::function<void(size_t)> progress_handler);
} // namespace yakka
