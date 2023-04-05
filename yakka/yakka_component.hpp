#pragma once

#include "yakka_blueprint.hpp"
#include "blueprint_database.hpp"
#include "yaml-cpp/yaml.h"
#include "yakka.hpp"
#include <vector>
#include <string>
#include <iostream>
#include <filesystem>

namespace yakka {

struct base_component {
  void parse_file(fs::path file_path);
  std::tuple<component_list_t &, feature_list_t &> apply_feature(std::string feature_name);
  std::tuple<component_list_t &, feature_list_t &> process_requirements(const YAML::Node &node);
  component_list_t get_required_components();
  feature_list_t get_required_features();
  // std::vector< blueprint_node > get_blueprints();

  // Variables
  fs::path file_path;
};

struct component : public base_component {
  YAML::Node &parse_file(fs::path file_path, blueprint_database &database);
  component_list_t get_required_components();
  feature_list_t get_required_features();
  // std::vector< blueprint_node > get_blueprints();

  std::string id;
  YAML::Node yaml;
};

struct slcc : public base_component {
  static YAML::Node slcc_database;

  slcc(){};
  slcc(fs::path file_path);

  void parse_file(fs::path file_path);
  // std::tuple<component_list_t&, feature_list_t&> apply_feature( std::string feature_name );
  // std::tuple<component_list_t&, feature_list_t&> process_requirements(const YAML::Node& node);
  component_list_t get_required_components();
  feature_list_t get_required_features();
  // std::vector< blueprint_node > get_blueprints();

  void convert_to_yakka();

  YAML::Node yaml;
};

} /* namespace yakka */
