#pragma once

#include "yaml-cpp/yaml.h"
#include "inja.hpp"
#include "component_database.hpp"
#include <string>
#include <future>
#include <optional>
#include <filesystem>

namespace yakka {
class workspace {
public:
  workspace();
  void init(fs::path workspace_path = ".");
  std::future<fs::path> fetch_component(const std::string &name, YAML::Node node, std::function<void(std::string, size_t)> progress_handler);
  void load_component_registries();
  yakka_status add_component_registry(const std::string &url);
  std::optional<YAML::Node> find_registry_component(const std::string &name);
  std::optional<fs::path> find_component(const std::string component_dotname);
  void load_config_file(const fs::path config_file_path);
  std::string template_render(const std::string input);
  yakka_status fetch_registry(const std::string &url);
  yakka_status update_component(const std::string &name);
  fs::path get_yakka_shared_home();

  static std::filesystem::path do_fetch_component(const std::string &name,
                                                  const std::string url,
                                                  const std::string branch,
                                                  const fs::path git_location,
                                                  const fs::path checkout_location,
                                                  std::function<void(std::string, size_t)> progress_handler);

public:
  std::shared_ptr<spdlog::logger> log;
  YAML::Node registries;
  YAML::Node configuration;
  nlohmann::json configuration_json;
  std::map<std::string, std::future<void>> fetching_list;
  std::filesystem::path workspace_path;
  std::filesystem::path shared_components_path;
  inja::Environment inja_environment;
  component_database local_database;
  component_database shared_database;
  std::filesystem::path yakka_shared_home;
};
} // namespace yakka
