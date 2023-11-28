#pragma once
#include "nlohmann/json.hpp"
#include <unordered_set>
#include <map>
#include <filesystem>

class slc_sdk {
public:
  std::filesystem::path root_path;
  //std::map<std::string, YAML::Node> component_database;
  nlohmann::json database;
  //std::multimap<std::string, std::string> provide_database;

  slc_sdk(std::filesystem::path sdk_root_path)
  {
    root_path = sdk_root_path;
  }

  void refresh_database(const std::string path = ".");
  void load_database();
  void save_database();
};