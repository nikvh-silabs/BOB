#include "yakka_blueprint.hpp"
#include <iostream>

namespace yakka {
blueprint::blueprint(const std::string &target, const nlohmann::json &blueprint, const std::string &parent_path)
{
  this->target      = target;
  this->parent_path = parent_path;

  if (blueprint.contains("regex"))
    this->regex = blueprint["regex"].get<std::string>();

  if (blueprint.contains("depends"))
    for (auto &d: blueprint["depends"]) {
      if (d.is_primitive())
        this->dependencies.push_back({ dependency::DEFAULT_DEPENDENCY, d.get<std::string>() });
      else if (d.is_object()) {
        if (d.contains("data")) {
          if (d["data"].is_array())
            for (auto &i: d["data"])
              this->dependencies.push_back({ dependency::DATA_DEPENDENCY, i.get<std::string>() });
          else
            this->dependencies.push_back({ dependency::DATA_DEPENDENCY, d["data"].get<std::string>() });
        } else if (d.contains("dependency_file")) {
          this->dependencies.push_back({ dependency::DEPENDENCY_FILE_DEPENDENCY, d["dependency_file"].get<std::string>() });
        }
      }
    }

  if (blueprint.contains("process"))
    process = blueprint["process"];
}
} // namespace yakka