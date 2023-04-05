#pragma once
#include "yaml-cpp/yaml.h"
#include <unordered_set>
#include <map>

class slc_project {
public:
  std::multimap<std::string, YAML::Node> provided_requirements;
  std::map<std::string, YAML::Node> slcc_database;

  void resolve_project(std::unordered_set<std::string> components);
  void generate_slcc_database(const std::string path);
};