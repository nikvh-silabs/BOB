#include "nlohmann/json.hpp"
#include "yaml-cpp/yaml.h"
#include <filesystem>

struct slcc {
  static YAML::Node slcc_database;

  std::filesystem::path file_path;

  slcc(std::filesystem::path file_path);

  void parse_file(std::filesystem::path file_path);
  // std::tuple<component_list_t&, feature_list_t&> apply_feature( std::string feature_name );
  // std::tuple<component_list_t&, feature_list_t&> process_requirements(const YAML::Node& node);
  //component_list_t get_required_components();
  //feature_list_t get_required_features();
  // std::vector< blueprint_node > get_blueprints();

  void convert_to_yakka();

  nlohmann::json json;
  YAML::Node yaml;
};