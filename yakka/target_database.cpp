#include "target_database.hpp"
#include "blueprint_database.hpp"
#include "inja/inja.hpp"
#include "yakka.hpp"
#include <regex>

namespace yakka {

void target_database::add_to_target_database(const std::string target, blueprint_database &blueprint_database, YAML::Node project_summary, nlohmann::json project_summary)
{
  bool blueprint_match_found = false;

  for (const auto &blueprint: blueprint_database.blueprints) {
    blueprint_match match;

    // Check if rule is a regex, otherwise do a string comparison
    if (blueprint.second->regex.has_value()) {
      std::smatch s;
      if (!std::regex_match(target, s, std::regex{ blueprint.first }))
        continue;

      // arg_count starts at 0 as the first match is the entire string
      for (auto &regex_match: s)
        match.regex_matches.push_back(regex_match.str());
    } else {
      if (target != blueprint.first)
        continue;

      match.regex_matches.push_back(target);
    }

    // Found a match. Create a blueprint match object
    blueprint_match_found = true;
    match.blueprint       = blueprint.second;

    inja::Environment local_inja_env;
    local_inja_env.add_callback("$", 1, [&match](const inja::Arguments &args) {
      return match.regex_matches[args[0]->get<int>()];
    });

    local_inja_env.add_callback("curdir", 0, [&match](const inja::Arguments &args) {
      return match.blueprint->parent_path;
    });
    local_inja_env.add_callback("render", 1, [&](const inja::Arguments &args) {
      return try_render(local_inja_env, args[0]->get<std::string>(), project_summary, log);
    });
    local_inja_env.add_callback("aggregate", 1, [&](const inja::Arguments &args) {
      YAML::Node aggregate;
      const std::string path = args[0]->get<std::string>();
      // Loop through components, check if object path exists, if so add it to the aggregate
      for (const auto &c: project_summary["components"]) {
        auto v = yaml_path(c.second, path);
        if (!v)
          continue;

        if (v.IsMap())
          for (auto i: v)
            aggregate[i.first] = i.second; //try_render(local_inja_env, i.second.as<std::string>(), this->project_summary, log);
        else if (v.IsSequence())
          for (auto i: v)
            aggregate.push_back(try_render(local_inja_env, i.as<std::string>(), project_summary, log));
        else
          aggregate.push_back(try_render(local_inja_env, v.as<std::string>(), project_summary, log));
      }
      if (aggregate.IsNull())
        return nlohmann::json();
      else
        return aggregate.as<nlohmann::json>();
    });

    // Run template engine on dependencies
    for (auto d: blueprint.second->dependencies) {
      switch (d.type) {
        case blueprint::dependency::DEPENDENCY_FILE_DEPENDENCY: {
          const std::string generated_dependency_file = try_render(local_inja_env, d.name, project_summary, log);
          auto dependencies                           = parse_gcc_dependency_file(generated_dependency_file);
          match.dependencies.insert(std::end(match.dependencies), std::begin(dependencies), std::end(dependencies));
          continue;
        }
        case blueprint::dependency::DATA_DEPENDENCY: {
          std::string data_name = try_render(local_inja_env, d.name, project_summary, log);
          if (data_name.front() != data_dependency_identifier)
            data_name.insert(0, 1, data_dependency_identifier);
          match.dependencies.push_back(data_name);
          continue;
        }
        default:
          break;
      }

      // Generate full dependency string by applying template engine
      std::string generated_depend;
      try {
        generated_depend = try_render(local_inja_env, d.name, project_summary, log);
      } catch (std::exception &e) {
        log->error("Couldn't apply template: '{}'", d.name);
        return;
      }

      // Check if the input was a YAML array construct
      if (generated_depend.front() == '[' && generated_depend.back() == ']') {
        // Load the generated dependency string as YAML and push each item individually
        try {
          auto generated_node = YAML::Load(generated_depend);
          for (auto i: generated_node)
            match.dependencies.push_back(i.Scalar());
        } catch (std::exception &e) {
          std::cerr << "Failed to parse dependency: " << d.name << "\n";
        }
      } else {
        match.dependencies.push_back(generated_depend);
      }
    }

    target_database.insert(std::make_pair(target, std::move(match)));
  }

  if (!blueprint_match_found) {
    if (!fs::exists(target))
      log->info("No blueprint for '{}'", target);
    // task_database.insert(std::make_pair(target, std::make_shared<blueprint_node>(target)));
  }
}

void target_database::generate_target_database(std::vector<std::string> commands)
{
  std::vector<std::string> new_targets;
  std::unordered_set<std::string> processed_targets;
  std::vector<std::string> unprocessed_targets;

  for (const auto &c: commands)
    unprocessed_targets.push_back(c);

  while (!unprocessed_targets.empty()) {
    for (const auto &t: unprocessed_targets) {
      // Add to processed targets and check if it's already been processed
      if (processed_targets.insert(t).second == false)
        continue;

      // Do not add to task database if it's a data dependency. There is special processing of these.
      if (t.front() == yakka::data_dependency_identifier)
        continue;

      // Check if target is not in the database. Note task_database is a multimap
      if (targets.find(t) == targets.end()) {
        const auto match = blueprint_database.find_match(t, this->project_summary);
        targets.insert({ t, match });
      }
      auto tasks = targets.equal_range(t);

      std::for_each(tasks.first, tasks.second, [&new_targets](auto &i) {
        if (i.second)
          new_targets.insert(new_targets.end(), i.second->dependencies.begin(), i.second->dependencies.end());
      });
    }

    unprocessed_targets.clear();
    unprocessed_targets.swap(new_targets);
  }
}
} // namespace yakka