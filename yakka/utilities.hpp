#pragma once

#include "yaml-cpp/yaml.h"
#include "inja.hpp"
#include "spdlog/spdlog.h"
#include <string>
#include <unordered_set>
#include <filesystem>

namespace fs = std::filesystem;

namespace yakka {
using component_list_t = std::unordered_set<std::string>;
using feature_list_t   = std::unordered_set<std::string>;
using command_list_t   = std::unordered_set<std::string>;

std::pair<std::string, int> exec(const std::string &command_text, const std::string &arg_text);
int exec(const std::string &command_text, const std::string &arg_text, std::function<void(std::string &)> function);
bool yaml_diff(const YAML::Node &node1, const YAML::Node &node2);
void yaml_node_merge(YAML::Node &merge_target, const YAML::Node &node);
void json_node_merge(nlohmann::json &merge_target, const nlohmann::json &node);
YAML::Node yaml_path(const YAML::Node &node, std::string path);
nlohmann::json json_path(const nlohmann::json &node, std::string path);
nlohmann::json::json_pointer json_pointer(std::string path);
std::tuple<component_list_t, feature_list_t, command_list_t> parse_arguments(const std::vector<std::string> &argument_string);
std::string generate_project_name(const component_list_t &components, const feature_list_t &features);
std::vector<std::string> parse_gcc_dependency_file(const std::string filename);
std::string component_dotname_to_id(const std::string dotname);
fs::path get_yakka_shared_home();
std::string try_render(inja::Environment &env, const std::string &input, const nlohmann::json &data, std::shared_ptr<spdlog::logger> log);
std::string try_render_file(inja::Environment &env, const std::string &filename, const nlohmann::json &data, std::shared_ptr<spdlog::logger> log);
} // namespace yakka