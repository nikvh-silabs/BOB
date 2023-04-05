#pragma once

#include "yakka_blueprint.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace yakka {
struct blueprint_match {
  std::vector<std::string> dependencies; // Template processed dependencies
  std::shared_ptr<yakka::blueprint> blueprint;
  std::vector<std::string> regex_matches; // Regex capture groups for a particular regex match
};

class blueprint_database {
public:
  void load(const std::string path);
  void save(const std::string path);
  std::shared_ptr<blueprint_match> find_match(const std::string target, const nlohmann::json &project_summary);

  // void generate_task_database(std::vector<std::string> command_list);
  // void process_blueprint_target( const std::string target );

  std::multimap<std::string, std::shared_ptr<blueprint>> blueprints;
};

class target_database {
public:
  void load(const fs::path file_path);
  void save(const fs::path file_path);

  std::multimap<std::string, std::shared_ptr<blueprint_match>> targets;
};
} // namespace yakka
