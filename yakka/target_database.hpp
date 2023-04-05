#pragma once

#include "yakka_blueprint.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace yakka {
class target_database {
public:
  void load(const std::string path);
  void save(const std::string path);

  void add_to_target_database(const std::string target);
  void generate_target_database();

  std::multimap<std::string, blueprint_match> target_database;
};
} // namespace yakka
