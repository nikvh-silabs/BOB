#include "yakka_component.hpp"
#include "blueprint_database.hpp"
#include "spdlog/spdlog.h"

namespace yakka {
YAML::Node &component::parse_file(fs::path file_path, blueprint_database &database)
{
  auto yakkalog           = spdlog::get("yakkalog");
  this->file_path         = file_path;
  std::string path_string = file_path.generic_string();
  yakkalog->info("Parsing '{}'", path_string);

  try {
    yaml = YAML::LoadFile(path_string);
  } catch (std::exception &e) {
    yakkalog->error("Failed to load file: '{}'\n{}\n", path_string, e.what());
    std::cerr << "Failed to parse: " << path_string << "\n" << e.what() << "\n";
    return yaml;
  }

  // Add known information
  this->id           = file_path.stem().string();
  yaml["yakka_file"] = path_string;

  if (file_path.has_parent_path())
    path_string = file_path.parent_path().generic_string();
  else
    path_string = ".";
  yaml["directory"] = path_string;

  // Ensure certain nodes are sequences
  if (yaml["requires"]["components"].IsScalar()) {
    std::string value              = yaml["requires"]["components"].Scalar();
    yaml["requires"]["components"] = YAML::Node();
    yaml["requires"]["components"].push_back(value);
  }

  if (yaml["requires"]["features"].IsScalar()) {
    std::string value            = yaml["requires"]["features"].Scalar();
    yaml["requires"]["features"] = YAML::Node();
    yaml["requires"]["features"].push_back(value);
  }

  // Fix relative component addressing
  for (auto n: yaml["requires"]["components"])
    if (n.Scalar().front() == '.')
      n.first = n.as<std::string>().insert(0, path_string);

  for (auto f: yaml["supports"]["features"])
    for (auto n: f["requires"]["components"])
      if (n.Scalar().front() == '.')
        n.first = n.as<std::string>().insert(0, path_string);

  for (auto c: yaml["supports"]["components"])
    for (auto n: c["requires"]["components"])
      if (n.Scalar().front() == '.')
        n.first = n.as<std::string>().insert(0, path_string);

  return yaml;
}

} /* namespace yakka */
