#pragma once

#include "yaml-cpp/yaml.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace yakka {
class component_database : public YAML::Node {
public:
  component_database();
  virtual ~component_database();

  void insert(const std::string id, fs::path config_file);
  void load(fs::path workspace_path);
  void save();
  void erase();
  void clear();
  void add_component(fs::path path);
  void scan_for_components(fs::path search_start_path = "");

  bool has_scanned;

private:
  fs::path workspace_path;
  fs::path database_filename;
  bool database_is_dirty;
};

} /* namespace yakka */
