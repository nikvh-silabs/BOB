#pragma once

#include "yakka.hpp"
#include "yakka_component.hpp"
#include "yakka_workspace.hpp"
#include "component_database.hpp"
#include "blueprint_database.hpp"
#include "yaml-cpp/yaml.h"
#include "nlohmann/json.hpp"
#include "inja.hpp"
#include "spdlog/spdlog.h"
#include "indicators/progress_bar.hpp"
#include "taskflow.hpp"
#include <filesystem>
#include <regex>
#include <map>
#include <unordered_set>
#include <optional>
#include <functional>

namespace fs = std::filesystem;

namespace yakka {
const std::string default_output_directory = "output/";

typedef std::function<yakka::process_return(std::string, const nlohmann::json &, std::string, const nlohmann::json &, inja::Environment &)> blueprint_command;

struct construction_task {
  std::shared_ptr<blueprint_match> match;
  fs::file_time_type last_modified;
  tf::Task task;
  // construction_task_state state;
  // std::future<std::pair<std::string, int>> thread_result;

  construction_task() : match(nullptr), last_modified(fs::file_time_type::min())
  {
  }
};

class project {
public:
  enum class state { PROJECT_HAS_UNKNOWN_COMPONENTS, PROJECT_HAS_REMOTE_COMPONENTS, PROJECT_HAS_INVALID_COMPONENT, PROJECT_HAS_MULTIPLE_REPLACEMENTS, PROJECT_VALID };

public:
  project(const std::string project_name, yakka::workspace &workspace, std::shared_ptr<spdlog::logger> log = nullptr);

  virtual ~project();

  void set_project_directory(const std::string path);
  void init_project(std::vector<std::string> components, std::vector<std::string> features);
  void init_project(const std::string build_string);
  void process_build_string(const std::string build_string);
  void parse_project_string(const std::vector<std::string> &project_string);
  void process_requirements(YAML::Node &component, YAML::Node child_node);

  void evaluate(bool evaluate_dependencies, bool fetch_components);
  state evaluate_dependencies();
  void evaluate_choices();
  std::shared_ptr<yakka::component> add_component(const std::string name);
  void process_component_dependencies(std::shared_ptr<yakka::component> new_component);

  void parse_blueprints();
  void update_summary();
  void generate_project_summary();

  // Target database management
  //void add_to_target_database( const std::string target );
  void generate_target_database();

  void load_common_commands();
  void set_project_file(const std::string filepath);
  void process_construction(indicators::ProgressBar &bar);
  void save_summary();
  void save_blueprints();
  bool has_data_dependency_changed(std::string data_path, const nlohmann::json left, const nlohmann::json right);
  void create_tasks(const std::string target_name, tf::Task &parent);

  // Logging
  std::shared_ptr<spdlog::logger> log;

  // Basic project data
  std::string project_name;
  std::string output_path;
  std::string yakka_home_directory;
  std::vector<std::string> initial_components;
  std::vector<std::string> initial_features;

  // Component processing
  std::unordered_set<std::string> unprocessed_components;
  std::unordered_set<std::string> unprocessed_features;
  std::unordered_set<std::string> unprocessed_choices;
  //std::unordered_set<std::string> replaced_components;
  std::unordered_map<std::string, std::string> replacements;
  std::unordered_set<std::string> required_components;
  std::unordered_set<std::string> required_features;
  std::unordered_set<std::string> commands;
  std::unordered_set<std::string> unknown_components;
  std::vector<std::pair<std::string, std::string>> incomplete_choices;
  std::vector<std::string> multiple_answer_choices;

  YAML::Node project_summary_yaml;
  std::string project_directory;
  std::string project_summary_file;
  fs::file_time_type project_summary_last_modified;
  std::vector<std::shared_ptr<yakka::component>> components;
  //yakka::component_database component_database;
  yakka::blueprint_database blueprint_database;
  yakka::target_database target_database;

  nlohmann::json previous_summary;
  nlohmann::json project_summary;

  workspace &workspace;

  // Blueprint evaluation
  inja::Environment inja_environment;
  //std::multimap<std::string, std::shared_ptr<blueprint_match> > target_database;
  std::multimap<std::string, construction_task> todo_list;
  int work_task_count;
  tf::Taskflow taskflow;
  std::atomic<bool> abort_build;

  std::map<std::string, blueprint_command> blueprint_commands;
  std::function<void()> task_complete_handler;

private:
  void init_project();
};

//std::string try_render(inja::Environment& env, const std::string& input, const nlohmann::json& data, std::shared_ptr<spdlog::logger> log);
std::pair<std::string, int> run_command(const std::string target, construction_task *task, project *project);
} /* namespace yakka */
