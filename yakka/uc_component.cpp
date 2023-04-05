#include "yakka_component.h"
#include "yaml-cpp/yaml.h"
#include "spdlog/spdlog.h"

namespace yakka {
YAML::Node slcc::slcc_database;

slcc::slcc(fs::path file_path)
{
  parse_file(file_path);
}

void slcc::parse_file(fs::path file_path)
{
  auto yakkalog           = spdlog::get("yakkalog");
  this->file_path         = file_path;
  std::string path_string = file_path.generic_string();
  yakkalog->info("Parsing '{}", path_string);

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
    yakkalog->error("Failed to load file: '{}'", path_string);
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

component_list_t slcc::get_required_components()
{
  return {};
}

feature_list_t slcc::get_required_features()
{
  feature_list_t list;
  for (const auto &f: yaml["requires"]["features"])
    list.insert(f.Scalar());

  return list;
}

std::vector<blueprint_node> slcc::get_blueprints()
{
  return {};
}

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
    for (const auto &p: yaml["source"])
      sources.push_back(p["path"]);
    yaml["sources"] = sources;
    yaml.remove("source");
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
    yaml.remove("include");
  }
}
} // namespace yakka
