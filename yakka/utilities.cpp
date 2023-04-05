#include "yakka_project.hpp"
#include "utilities.hpp"
#include "subprocess.hpp"
#include "spdlog/spdlog.h"
#include "glob/glob.h"
#include <fstream>

namespace yakka {

std::pair<std::string, int> exec(const std::string &command_text, const std::string &arg_text)
{
  auto yakkalog = spdlog::get("yakkalog");
  yakkalog->info("{} {}", command_text, arg_text);
  try {
    std::string command = command_text;
    if (!arg_text.empty())
      command += " " + arg_text;
#if defined(__USING_WINDOWS__)
    auto p = subprocess::Popen(command, subprocess::output{ subprocess::PIPE }, subprocess::error{ subprocess::STDOUT });
#else
    auto p = subprocess::Popen(command, subprocess::shell{ true }, subprocess::output{ subprocess::PIPE }, subprocess::error{ subprocess::STDOUT });
#endif
    auto output = p.output();
    std::string result;
    if (output != nullptr) {
      while (1) {
        int c = fgetc(output);
        if (c == EOF)
          break;
        result += (char)c;
      };
    }

    auto retcode = p.wait();
#if defined(__USING_WINDOWS__)
    retcode = p.poll();
#endif
    return { result, retcode };
  } catch (std::exception e) {
    yakkalog->error("Exception while executing: {}\n{}", command_text, e.what());
    return { "", -1 };
  }
}

int exec(const std::string &command_text, const std::string &arg_text, std::function<void(std::string &)> function)
{
  auto yakkalog = spdlog::get("yakkalog");
  yakkalog->info("{} {}", command_text, arg_text);
  try {
    std::string command = command_text;
    if (!arg_text.empty())
      command += " " + arg_text;
#if defined(__USING_WINDOWS__)
    auto p = subprocess::Popen(command, subprocess::output{ subprocess::PIPE }, subprocess::error{ subprocess::STDOUT });
#else
    auto p = subprocess::Popen(command, subprocess::shell{ true }, subprocess::output{ subprocess::PIPE }, subprocess::error{ subprocess::STDOUT });
#endif
    auto output = p.output();
    std::array<char, 512> buffer;
    size_t count = 0;
    buffer.fill('\0');
    if (output != nullptr) {
      while (1) {
        buffer[count] = fgetc(output);
        if (feof(output))
          break;

        if (count == buffer.size() - 1 || buffer[count] == '\r' || buffer[count] == '\n') {
          std::string temp(buffer.data());
          try {
            function(temp);
          } catch (std::exception e) {
            yakkalog->debug("exec() data processing threw exception '{}'for the following data:\n{}", e.what(), temp);
          }
          buffer.fill('\0');
          count = 0;
        } else
          ++count;
      };
    }
    auto retcode = p.wait();
#if defined(__USING_WINDOWS__)
    retcode = p.poll();
#endif
    return retcode;
  } catch (std::exception e) {
    yakkalog->error("Exception while executing: {}\n{}", command_text, e.what());
  }
  return -1;
}

bool yaml_diff(const YAML::Node &node1, const YAML::Node &node2)
{
  std::vector<std::pair<const YAML::Node &, const YAML::Node &>> compare_list;
  compare_list.push_back({ node1, node2 });
  for (int i = 0; i < compare_list.size(); ++i) {
    const YAML::Node &left  = compare_list[i].first;
    const YAML::Node &right = compare_list[i].second;

    if (left.Type() != right.Type())
      return true;

    switch (left.Type()) {
      case YAML::NodeType::Scalar:
        if (left.Scalar() != right.Scalar())
          return true;
        break;

      case YAML::NodeType::Sequence:
        // Verify the sequences have the same length
        if (left.size() != right.size())
          return true;
        for (int a = 0; a < left.size(); ++a)
          compare_list.push_back({ left[a], right[a] });
        break;

      case YAML::NodeType::Map:
        // Verify the maps have the same length
        if (left.size() != right.size())
          return true;
        for (const auto &a: left) {
          auto &key  = a.first.Scalar();
          auto &node = a.second;
          if (!right[key])
            return true;
          compare_list.push_back({ node, right[key] });
        }
        break;
      default:
        break;
    }
  }

  return false;
}

YAML::Node yaml_path(const YAML::Node &node, std::string path)
{
  YAML::Node temp = node;
  std::stringstream ss(path);
  std::string s;
  while (std::getline(ss, s, '.'))
    temp.reset(temp[s]);
  return temp;
}

nlohmann::json::json_pointer json_pointer(std::string path)
{
  if (path.front() != '/') {
    path = "/" + path;
    std::replace(path.begin(), path.end(), '.', '/');
  }
  return nlohmann::json::json_pointer{ path };
}

nlohmann::json json_path(const nlohmann::json &node, std::string path)
{
  nlohmann::json::json_pointer temp(path);
  return node[temp];

  // return node[nlohmann::json_pointer(path)];
  // auto temp = node;
  // std::stringstream ss(path);
  // std::string s;
  // while (std::getline(ss, s, '.'))
  //     temp = temp[s];//.reset(temp[s]);
  // return temp;
}

std::tuple<component_list_t, feature_list_t, command_list_t> parse_arguments(const std::vector<std::string> &argument_string)
{
  component_list_t components;
  feature_list_t features;
  command_list_t commands;

  //    for (auto s = argument_string.begin(); s != argument_string.end(); ++s)
  for (auto s: argument_string) {
    // Identify features, commands, and components
    if (s.front() == '+')
      features.insert(s.substr(1));
    else if (s.back() == '!')
      commands.insert(s.substr(0, s.size() - 1));
    else
      components.insert(s);
  }

  return { std::move(components), std::move(features), std::move(commands) };
}

std::string generate_project_name(const component_list_t &components, const feature_list_t &features)
{
  std::string project_name = "";

  // Generate the project name from the project string
  for (const auto &i: components)
    project_name += i + "-";

  if (!components.empty())
    project_name.pop_back();

  for (const auto &i: features)
    project_name += "-" + i;

  if (project_name.empty())
    project_name = "none";

  return project_name;
}

/**
 * @brief Parses dependency files as output by GCC or Clang generating a vector of filenames as found in the named file
 *
 * @param filename  Name of the dependency file. Typically ending in '.d'
 * @return std::vector<std::string>  Vector of files specified as dependencies
 */
std::vector<std::string> parse_gcc_dependency_file(const std::string filename)
{
  std::vector<std::string> dependencies;
  std::ifstream infile(filename);

  if (!infile.is_open())
    return {};

  std::string line;

  // Find and ignore the first line with the target. Typically "<target>: \"
  do {
    std::getline(infile, line);
  } while (line.length() > 0 && line.find(':') == std::string::npos);

  while (std::getline(infile, line, ' ')) {
    if (line.empty() || line.compare("\\\n") == 0)
      continue;
    if (line.back() == '\n')
      line.pop_back();
    if (line.back() == '\r')
      line.pop_back();
    dependencies.push_back(line.starts_with("./") ? line.substr(2) : line);
  }

  return dependencies;
}

void yaml_node_merge(YAML::Node &merge_target, const YAML::Node &node)
{
  auto yakkalog = spdlog::get("yakkalog");
  if (!node.IsMap()) {
    yakkalog->error("Invalid feature node {}", node.as<std::string>());
    return;
  }

  for (const auto &i: node) {
    const std::string item_name = i.first.as<std::string>();
    YAML::Node item_node        = i.second;

    if (!merge_target[item_name]) {
      merge_target[item_name] = item_node;
    } else {
      if (item_node.IsScalar()) {
        if (merge_target[item_name].IsScalar()) {
          YAML::Node new_node;
          new_node.push_back(merge_target[item_name]);
          new_node.push_back(item_node.Scalar());
          merge_target[item_name].reset(new_node);
        } else if (merge_target[item_name].IsSequence()) {
          merge_target[item_name].push_back(item_node.Scalar());
        } else {
          yakkalog->error("Cannot merge scalar and map\nScalar: {}\nMap: {}", i.first.as<std::string>(), merge_target[item_name].as<std::string>());
          return;
        }
      } else if (item_node.IsSequence()) {
        if (merge_target[item_name].IsMap()) {
          yakkalog->error("Cannot merge sequence and map\n{}\n{}", merge_target.as<std::string>(), node.as<std::string>());
          return;
        }
        if (merge_target[item_name].IsScalar()) {
          // Convert merge_target from a scalar to a sequence
          YAML::Node new_node;
          new_node.push_back(merge_target[item_name].Scalar());
          merge_target[item_name] = new_node;
        }
        for (auto a: item_node) {
          merge_target[item_name].push_back(a);
        }
      } else if (item_node.IsMap()) {
        if (!merge_target[item_name].IsMap()) {
          yakkalog->error("Cannot merge map and non-map\n{}\n{}", merge_target.as<std::string>(), node.as<std::string>());
          return;
        }
        auto new_merge = merge_target[item_name];
        yaml_node_merge(new_merge, item_node);
      }
    }
  }
}

void json_node_merge(nlohmann::json &merge_target, const nlohmann::json &node)
{
  auto yakkalog = spdlog::get("yakkalog");
  switch (node.type()) {
    case nlohmann::detail::value_t::object:
      switch (merge_target.type()) {
        case nlohmann::detail::value_t::object:
        case nlohmann::detail::value_t::array:
        default:
          yakkalog->error("Currently not supported");
          break;
      }
      break;
    case nlohmann::detail::value_t::array:
      switch (merge_target.type()) {
        case nlohmann::detail::value_t::object:
          yakkalog->error("Cannot merge array into an object");
          break;
        case nlohmann::detail::value_t::array:
          for (auto &i: node)
            merge_target.push_back(i);
          break;
        default:
          merge_target.push_back(node);
          break;
      }
      break;
    default:
      switch (merge_target.type()) {
        case nlohmann::detail::value_t::object:
          yakkalog->error("Cannot merge scalar into an object");
          break;
        case nlohmann::detail::value_t::array:
        default:
          merge_target.push_back(node);
          break;
      }
      break;
  }
}

std::string component_dotname_to_id(const std::string dotname)
{
  return dotname.find_last_of(".") != std::string::npos ? dotname.substr(dotname.find_last_of(".") + 1) : dotname;
}

std::string try_render(inja::Environment &env, const std::string &input, const nlohmann::json &data, std::shared_ptr<spdlog::logger> log = nullptr)
{
  try {
    return env.render(input, data);
  } catch (std::exception &e) {
    if (log != nullptr)
      log->error("Template error: {}\n{}", input, e.what());

    std::cerr << "Template error: " << input << "\n" << e.what() << "\n";
    //exit(-1);
    return "";
  }
}

std::string try_render_file(inja::Environment &env, const std::string &filename, const nlohmann::json &data, std::shared_ptr<spdlog::logger> log = nullptr)
{
  try {
    return env.render_file(filename, data);
  } catch (std::exception &e) {
    if (log != nullptr)
      log->error("Template error: {}\n{}", filename, e.what());

    std::cerr << "Template error: " << filename << "\n" << e.what() << "\n";
    //exit(-1);
    return "";
  }
}

std::pair<std::string, int> run_command(const std::string target, construction_task *task, project *project)
{
  auto yakkalog               = spdlog::get("yakkalog");
  auto console                = spdlog::get("yakkaconsole");
  std::string captured_output = "";
  inja::Environment inja_env  = inja::Environment();
  auto &blueprint             = task->match;

  std::string curdir_path = blueprint->blueprint->parent_path;
  inja_env.add_callback("$", 1, [&blueprint](const inja::Arguments &args) {
    return blueprint->regex_matches[args[0]->get<int>()];
  });
  inja_env.add_callback("curdir", 0, [&](const inja::Arguments &args) {
    return curdir_path;
  });
  inja_env.add_callback("dir", 1, [](inja::Arguments &args) {
    auto path = std::filesystem::path{ args.at(0)->get<std::string>() }.relative_path();
    if (path.has_filename())
      return path.parent_path().string();
    else
      return path.string();
  });
  inja_env.add_callback("notdir", 1, [](inja::Arguments &args) {
    return std::filesystem::path{ args.at(0)->get<std::string>() }.filename();
  });
  inja_env.add_callback("glob", [](inja::Arguments &args) {
    nlohmann::json aggregate = nlohmann::json::array();
    std::vector<std::string> string_args;
    for (const auto &i: args)
      string_args.push_back(i->get<std::string>());
    for (auto &p: glob::rglob(string_args))
      aggregate.push_back(p.generic_string());
    return aggregate;
  });
  inja_env.add_callback("absolute_dir", 1, [](inja::Arguments &args) {
    return std::filesystem::absolute(args.at(0)->get<std::string>());
  });
  inja_env.add_callback("extension", 1, [](inja::Arguments &args) {
    return std::filesystem::path{ args.at(0)->get<std::string>() }.extension().string().substr(1);
  });
  inja_env.add_callback("filesize", 1, [&](const inja::Arguments &args) {
    return fs::file_size(args[0]->get<std::string>());
  });
  inja_env.add_callback("render", 1, [&](const inja::Arguments &args) {
    return try_render(inja_env, args[0]->get<std::string>(), project->project_summary);
  });
  inja_env.add_callback("render", 2, [&curdir_path, &inja_env, &project](const inja::Arguments &args) {
    auto backup               = curdir_path;
    curdir_path               = args[1]->get<std::string>();
    std::string render_output = try_render(inja_env, args[0]->get<std::string>(), project->project_summary);
    curdir_path               = backup;
    return render_output;
  });
  inja_env.add_callback("read_file", 1, [&](const inja::Arguments &args) {
    auto file = std::ifstream(args[0]->get<std::string>());
    return std::string{ std::istreambuf_iterator<char>{ file }, {} };
  });
  inja_env.add_callback("load_yaml", 1, [&](const inja::Arguments &args) {
    auto yaml_data = YAML::LoadFile(args[0]->get<std::string>());
    return yaml_data.as<nlohmann::json>();
  });
  inja_env.add_callback("aggregate", 1, [&](const inja::Arguments &args) {
    nlohmann::json aggregate;
    auto path = json_pointer(args[0]->get<std::string>());
    // Loop through components, check if object path exists, if so add it to the aggregate
    for (const auto &[c_key, c_value]: project->project_summary["components"].items()) {
      // auto v = json_path(c.value(), path);
      if (!c_value.contains(path) || c_value[path].is_null())
        continue;

      auto v = c_value[path];
      if (v.is_object())
        for (const auto &[i_key, i_value]: v.items()) {
          aggregate[i_key] = i_value; //try_render(inja_env, i.second.as<std::string>(), project->project_summary, log);
        }
      else if (v.is_array())
        for (const auto &[i_key, i_value]: v.items())
          if (i_value.is_object())
            aggregate.push_back(i_value);
          else
            aggregate.push_back(try_render(inja_env, i_value.get<std::string>(), project->project_summary));
      else if (!v.is_null())
        aggregate.push_back(try_render(inja_env, v.get<std::string>(), project->project_summary));
    }
    return aggregate;
  });

  std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

  // Note: A blueprint process is a sequence of maps
  for (const auto &command_entry: blueprint->blueprint->process) {
    assert(command_entry.is_object());

    if (command_entry.size() != 1) {
      console->error("Command '{}' for target '{}' is malformed", command_entry.begin().key(), target);
      return { "", -1 };
    }

    // Take the first entry in the map as the command
    auto command                   = command_entry.begin();
    const std::string command_name = command.key();
    int retcode                    = 0;

    try {
      if (project->project_summary["tools"].contains(command_name)) {
        auto tool                = project->project_summary["tools"][command_name];
        std::string command_text = "";

        command_text.append(tool);

        std::string arg_text = command.value().get<std::string>();

        // Apply template engine
        arg_text = try_render(inja_env, arg_text, project->project_summary, yakkalog);

        auto [temp_output, temp_retcode] = exec(command_text, arg_text);
        retcode                          = temp_retcode;

        if (retcode != 0) {
          console->error("\n{}", temp_output); // Ensure output starts on a new line
          yakkalog->error("Returned {}\n{}", retcode, temp_output);
          return { temp_output, retcode };
        }
        captured_output = temp_output;
        // Echo the output of the command
        // TODO: Note this should be done by the main thread to ensure the outputs from multiple run_command instances don't overlap
        yakkalog->info(captured_output);
      }
      // Else check if it is a built-in command
      else if (project->blueprint_commands.contains(command_name)) {
        yakka::process_return test_result = project->blueprint_commands.at(command_name)(target, command.value(), captured_output, project->project_summary, inja_env);
        captured_output                   = test_result.result;
        retcode                           = test_result.retcode;
      } else {
        yakkalog->error("{} tool doesn't exist", command_name);
      }

      if (retcode != 0)
        return { captured_output, retcode };
    } catch (std::exception &e) {
      yakkalog->error("Failed to run command: '{}' as part of {}", command_name, target);
      yakkalog->info("Failed to run: {}", command_entry.dump());
      throw e;
    }
  }

  std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
  auto duration                                     = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
  yakkalog->info("{}: {} milliseconds", target, duration);
  return { captured_output, 0 };
}

} // namespace yakka
