//#include "yakka_component.h"
#include "slc_component.hpp"
#include "yaml-cpp/yaml.h"
#include "spdlog/spdlog.h"
#include <filesystem>

YAML::Node slcc_database;

slcc::slcc(std::filesystem::path file_path)
{
  parse_file(file_path);
}

void slcc::parse_file(std::filesystem::path file_path)
{
  this->file_path         = file_path;
  std::string path_string = file_path.generic_string();
  spdlog::info("Parsing '{}", path_string);

  try {
    yaml = YAML::LoadFile(path_string);

    // Check if SLCC file is an omap. Convert it to a map
    if (yaml.IsSequence()) {
      YAML::Node new_format;
      for (const auto &it: yaml)
        new_format[it.begin()->first.Scalar()] = it.begin()->second;
      yaml.reset(new_format);
    }

    const std::string id = (yaml["id"]) ? yaml["id"].as<std::string>() : yaml["name"].as<std::string>();
    yaml["id"]           = id;

    if (yaml["root_path"])
      yaml["directory"] = yaml["root_path"];
  } catch (...) {
    spdlog::error("Failed to load file: '{}'", path_string);
    return;
  }
}

// void slcc::apply_feature( std::string feature_name, component_list_t& new_components, feature_list_t& new_features )
// {
//     // Nothing to do
// }

// void slcc::process_requirements(const YAML::Node& node, component_list_t& new_components, feature_list_t& new_features )
// {

// }

void slcc::convert_to_yakka()
{
  // Process 'provides'
  {
    YAML::Node provides;
    for (const auto &p: yaml["provides"]) {
      provides["features"].push_back(p["name"]);
    }
    yaml["provides"] = provides;
  }

  // Process 'requires'
  {
    YAML::Node temp;
    for (const auto &p: yaml["requires"]) {
      temp["features"].push_back(p["name"]);
    }
    yaml["requires"] = temp;
  }

  // Process 'source'
  {
    YAML::Node sources;
    for (const auto &p: yaml["source"]) {
      if (p["condition"]) {
        for (auto &c: p["condition"])
          yaml["supports"]["feature"][c.as<std::string>()]["sources"] = p["path"];
      } else {
        sources.push_back(p["path"]);
      }
    }
    yaml["sources"] = sources;
    //yaml.remove("source");
  }

  // Process 'include'
  {
    YAML::Node includes;
    for (const auto &p: yaml["include"]) {
      if (p["condition"])
        for (auto &c: p["condition"])
          yaml["supports"]["feature"][c.as<std::string>()]["includes"] = p["path"];
      else
        includes["global"].push_back(p["path"]);
    }
    yaml["includes"] = includes;
    //yaml.remove("include");
  }
}
