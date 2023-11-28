#pragma once
#include "yaml-cpp/yaml.h"
#include "slc_sdk.hpp"
#include <unordered_set>
#include <map>

class slc_project {
public:
  std::unordered_set<std::string> components;
  //std::multimap<std::string, YAML::Node> provided_requirements;

  void resolve_project(std::unordered_set<std::string> components, const slc_sdk &sdk);
};