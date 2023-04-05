#include "yakka.hpp"
#include "yakka_workspace.hpp"
#include "yakka_project.hpp"
#include "cxxopts.hpp"
#include "subprocess.hpp"
#include "spdlog/spdlog.h"
#include "semver.hpp"
#include <indicators/dynamic_progress.hpp>
#include <indicators/progress_bar.hpp>
#include <indicators/cursor_control.hpp>
#include <indicators/termcolor.hpp>
#include "taskflow.hpp"
#include "utilities.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <future>

using namespace indicators;
using namespace std::chrono_literals;

static void evaluate_project_dependencies(yakka::workspace &workspace, yakka::project &project);
static void evaluate_project_choices(yakka::workspace &workspace, yakka::project &project);
static void run_taskflow(yakka::project &project);

tf::Task &create_tasks(yakka::project &project, const std::string &name, std::map<std::string, tf::Task> &tasks, tf::Taskflow &taskflow);
static const semver::version yakka_version{
#include "yakka_version.h"
};

int main(int argc, char **argv)
{
  auto yakka_start_time = fs::file_time_type::clock::now();

  // Setup logging
  std::error_code error_code;
  fs::remove("yakka.log", error_code);

  auto console = spdlog::stderr_color_mt("yakkaconsole");
  console->flush_on(spdlog::level::level_enum::off);
  console->set_pattern("[%^%l%$]: %v");
  //spdlog::set_async_mode(4096);
  std::shared_ptr<spdlog::logger> yakkalog;
  try {
    yakkalog = spdlog::basic_logger_mt("yakkalog", "yakka.log");
  } catch (...) {
    try {
      auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
      yakkalog  = spdlog::basic_logger_mt("yakkalog", "yakka-" + std::to_string(time) + ".log");
    } catch (...) {
      std::cerr << "Cannot open yakka.log";
      exit(1);
    }
  }
  yakkalog->flush_on(spdlog::level::level_enum::trace);

  // Create a workspace
  yakka::workspace workspace;
  workspace.init(".");

  cxxopts::Options options("yakka", "Yakka the embedded builder. Ver " + yakka_version.to_string());
  options.allow_unrecognised_options();
  options.positional_help("<action> [optional args]");
  options.add_options()("h,help",
                        "Print usage")("r,refresh", "Refresh component database", cxxopts::value<bool>()->default_value("false"))("n,no-eval", "Skip the dependency and choice evaluation", cxxopts::value<bool>()->default_value("false"))(
    "o,no-output",
    "Do not generate output folder",
    cxxopts::value<bool>()->default_value(
      "false"))("f,fetch", "Fetch missing components", cxxopts::value<bool>()->default_value("false"))("action", "Select from 'register', 'list', 'update', 'git', or a command", cxxopts::value<std::string>());

  options.parse_positional({ "action" });
  auto result = options.parse(argc, argv);
  if (result.count("help") || argc == 1) {
    std::cout << options.help() << std::endl;
    return 0;
  }
  if (result["refresh"].as<bool>()) {
    workspace.local_database.erase();

    std::cout << "Scanning '.' for components\n";
    workspace.local_database.scan_for_components();
    workspace.local_database.save();
    std::cout << "Scan complete.\n";
  }

  // Check if there is no action. If so, print the help
  if (!result.count("action")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  auto action = result["action"].as<std::string>();
  if (action == "register") {
    if (result.unmatched().size() == 0) {
      console->error("Must provide URL of component registry");
      return -1;
    }
    // Ensure the BOB registries directory exists
    fs::create_directories(".yakka/registries");
    console->info("Adding component registry...");
    if (workspace.add_component_registry(result.unmatched()[0]) != yakka::yakka_status::SUCCESS) {
      console->error("Failed to add component registry. See yakka.log for details");
      return -1;
    }
    console->info("Complete");
    return 0;
  } else if (action == "list") {
    workspace.load_component_registries();
    for (auto registry: workspace.registries) {
      std::cout << registry.second["name"] << "\n";
      for (auto c: registry.second["provides"]["components"])
        std::cout << "  - " << c.first << "\n";
    }
    return 0;
  } else if (action == "update") {
    // Find all the component repos in .yakka
    //for (auto d: fs::directory_iterator(".yakka/repos"))
    for (auto &i: result.unmatched()) {
      // const auto name = d.path().filename().generic_string();
      std::cout << "Updating: " << i << "\n";
      workspace.update_component(i);
    }

    std::cout << "Complete\n";
    return 0;
  } else if (action == "git") {
    auto iter                 = result.unmatched().begin();
    const auto component_name = *iter;
    std::string git_command   = "--git-dir=.yakka/repos/" + component_name + "/.git --work-tree=components/" + component_name;
    for (iter++; iter != result.unmatched().end(); ++iter)
      if (iter->find(' ') == std::string::npos)
        git_command.append(" " + *iter);
      else
        git_command.append(" \"" + *iter + "\"");

    auto [output, result] = yakka::exec("git", git_command);
    std::cout << output;
    return 0;
  } else if (action.back() != '!') {
    std::cout << "Must provide an action or a command (commands end with !)\n";
    return 0;
  }

  // Action must be a command. Drop the !
  action.pop_back();

  // Process the command line options
  std::string project_name;
  std::string feature_suffix;
  std::vector<std::string> components;
  std::vector<std::string> features;
  std::vector<std::vector<std::string>> matrix_arguments;
  std::unordered_set<std::string> commands;
  bool processing_matrix_argument = false;
  std::vector<std::string> current_matrix_arg;
  for (auto s: result.unmatched()) {
    if (s.front() == '[') {
      s                          = s.substr(1);
      processing_matrix_argument = true;
      if (s.size() == 0)
        continue;
    }

    // Identify matrix arguments
    if (processing_matrix_argument) {
      size_t end_of_word;
      do {
        end_of_word = s.find_first_of(",]");
        current_matrix_arg.push_back(s.substr(0, end_of_word));

        if (s[end_of_word] == ']') {
          processing_matrix_argument = false;
          matrix_arguments.push_back(current_matrix_arg);
          current_matrix_arg.clear();
        }
        s = s.substr(end_of_word + 1);
      } while (end_of_word != std::string::npos && s.size() > 0);
    }
    // Identify features, commands, and components
    else if (s.front() == '+') {
      feature_suffix += s;
      features.push_back(s.substr(1));
    } else if (s.back() == '!')
      commands.insert(s.substr(0, s.size() - 1));
    else {
      components.push_back(s);

      // Compose the project name by concatenation all the components in CLI order.
      // The features will be added at the end
      project_name += s + "-";
    }
  }

  if (components.size() == 0) {
    console->error("No components identified");
    return -1;
  }

  // Generate project/s names
  for (const auto &matrix_arg: matrix_arguments)
    for (const auto &arg_option: matrix_arg)

      // Remove the extra "-" and add the feature suffix
      project_name.pop_back();
  project_name += feature_suffix;

  // Limit the project name to 64 characters
  if (project_name.length() > 64)
    project_name = project_name.substr(0, 64);

  // Create a project
  yakka::project project(project_name, workspace, yakkalog);

  // Move the CLI parsed data to the project
  // project.unprocessed_components = std::move(components);
  // project.unprocessed_features = std::move(features);
  project.commands = std::move(commands);

  // Add the action as a command
  project.commands.insert(action);

  // Init the project
  project.init_project(components, features);
  project.evaluate(!result["no-eval"].as<bool>(), result["fetch"].as<bool>());

  yakkalog->info("Required features:");
  for (auto f: project.required_features)
    yakkalog->info("- {}", f);

  project.generate_project_summary();
  project.save_summary();

  auto t1 = std::chrono::high_resolution_clock::now();
  project.parse_blueprints();
  project.generate_target_database();
  auto t2 = std::chrono::high_resolution_clock::now();

  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
  yakkalog->info("{}ms to process blueprints", duration);
  project.load_common_commands();

  run_taskflow(project);

  auto yakka_end_time = fs::file_time_type::clock::now();
  std::cout << "Complete in " << std::chrono::duration_cast<std::chrono::milliseconds>(yakka_end_time - yakka_start_time).count() << " milliseconds" << std::endl;

  console->flush();
  show_console_cursor(true);

  if (project.abort_build)
    return -1;
  else
    return 0;
}

void process_matrix_arguments(std::vector<std::vector<std::string>>::iterator iter, component_list_t components, std::vector<component_list_t> &output)
{
  auto &arg_list = *iter;
  for (const auto &i: arg_list) {
    component_list_t new_list = components;
    new_list.push_back(i) if (iter->next)
    {
      process_matrix_arguments(iter->next, new_list, output);
    }
    else
    {
      output.push_back(new_list);
    }
  }
}

void run_taskflow(yakka::project &project)
{
  project.work_task_count             = 0;
  std::atomic<int> execution_progress = 0;
  tf::Executor executor;
  auto finish = project.taskflow.emplace([&]() {
    execution_progress = 100;
  });
  for (auto &i: project.commands)
    project.create_tasks(i, finish);

  ProgressBar building_bar{ option::BarWidth{ 50 }, option::ShowPercentage{ true }, option::PrefixText{ "Building " }, option::MaxProgress{ project.work_task_count } };

  project.task_complete_handler = [&]() {
    ++execution_progress;
  };

  auto execution_future = executor.run(project.taskflow);

  do {
    building_bar.set_option(option::PostfixText{ std::to_string(execution_progress) + "/" + std::to_string(project.work_task_count) });
    building_bar.set_progress(execution_progress);
  } while (execution_future.wait_for(500ms) != std::future_status::ready);

  building_bar.set_option(option::PostfixText{ std::to_string(project.work_task_count) + "/" + std::to_string(project.work_task_count) });
  building_bar.set_progress(project.work_task_count);
}

static void evaluate_project_dependencies(yakka::workspace &workspace, yakka::project &project)
{
  auto console  = spdlog::get("yakkaconsole");
  auto yakkalog = spdlog::get("yakkalog");

  auto t1 = std::chrono::high_resolution_clock::now();

  if (project.evaluate_dependencies() == yakka::project::state::PROJECT_HAS_INVALID_COMPONENT)
    exit(1);

  // If there are still missing components, try and download them
  if (!project.unknown_components.empty()) {
    workspace.load_component_registries();

    show_console_cursor(false);
    DynamicProgress<ProgressBar> fetch_progress_ui;
    std::vector<std::shared_ptr<ProgressBar>> fetch_progress_bars;

    std::map<std::string, std::future<fs::path>> fetch_list;
    do {
      // Ask the workspace to fetch them
      for (const auto &i: project.unknown_components) {
        if (fetch_list.find(i) != fetch_list.end())
          continue;

        // Check if component is in the registry
        auto node = workspace.find_registry_component(i);
        if (node) {
          std::shared_ptr<ProgressBar> new_progress_bar = std::make_shared<ProgressBar>(option::BarWidth{ 50 }, option::ShowPercentage{ true }, option::PrefixText{ "Fetching " + i + " " }, option::SavedStartTime{ true });
          fetch_progress_bars.push_back(new_progress_bar);
          size_t id = fetch_progress_ui.push_back(*new_progress_bar);
          fetch_progress_ui.print_progress();
          auto result = workspace.fetch_component(i, *node, [&fetch_progress_ui, id](std::string prefix, size_t number) {
            fetch_progress_ui[id].set_option(option::PrefixText{ prefix });
            if (number >= 100) {
              fetch_progress_ui[id].set_progress(100);
              fetch_progress_ui[id].mark_as_completed();
            } else
              fetch_progress_ui[id].set_progress(number);
          });
          if (result.valid())
            fetch_list.insert({ i, std::move(result) });
        }
      }

      // Check if we haven't been able to fetch any of the unknown components
      if (fetch_list.empty()) {
        for (const auto &i: project.unknown_components)
          console->error("Cannot fetch {}", i);
        console->flush();
        yakkalog->flush();
        exit(0);
      }

      // Wait for one of the components to be complete
      decltype(fetch_list)::iterator completed_fetch;
      do {
        completed_fetch = std::find_if(fetch_list.begin(), fetch_list.end(), [](auto &fetch_item) {
          return fetch_item.second.wait_for(100ms) == std::future_status::ready;
        });
      } while (completed_fetch == fetch_list.end());

      auto new_component_path = completed_fetch->second.get();

      // Check if the fetch worked
      if (new_component_path.empty()) {
        yakkalog->error("Failed to fetch {}", completed_fetch->first);
        console->error("Failed to fetch {}", completed_fetch->first);
        project.unknown_components.erase(completed_fetch->first);
        fetch_list.erase(completed_fetch);
        continue;
      }

      // Update the component database
      if (new_component_path.string().starts_with(workspace.shared_components_path.string())) {
        yakkalog->info("Scanning for new component in shared database");
        workspace.shared_database.scan_for_components(new_component_path);
        workspace.shared_database.save();
      } else {
        yakkalog->info("Scanning for new component in local database");
        workspace.local_database.scan_for_components(new_component_path);
        workspace.shared_database.save();
      }

      // Check if any of our unknown components have been found
      for (auto i = project.unknown_components.cbegin(); i != project.unknown_components.cend();) {
        if (workspace.local_database[*i] || workspace.shared_database[*i]) {
          // Remove component from the unknown list and add it to the unprocessed list
          project.unprocessed_components.insert(*i);
          i = project.unknown_components.erase(i);
        } else
          ++i;
      }

      // Remove the item from the fetch list
      fetch_list.erase(completed_fetch);

      // Re-evaluate the project dependencies
      project.evaluate_dependencies();
    } while (!project.unprocessed_components.empty() || !project.unknown_components.empty() || !fetch_list.empty());
  }

  auto t2       = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
  yakkalog->info("{}ms to process components", duration);
}

static void evaluate_project_choices(yakka::workspace &workspace, yakka::project &project)
{
  auto console  = spdlog::get("yakkaconsole");
  auto yakkalog = spdlog::get("yakkalog");

  project.evaluate_choices();

  if (!project.incomplete_choices.empty()) {
    for (auto &i: project.incomplete_choices) {
      bool valid_options = false;
      console->error("Component '{}' has a choice '{}' - Must choose from the following", i.first, i.second);
      if (project.project_summary["choices"][i.second].contains("features")) {
        valid_options = true;
        console->error("Features: ");
        for (auto &b: project.project_summary["choices"][i.second]["features"])
          console->error("  - {}", b.get<std::string>());
      }

      if (project.project_summary["choices"][i.second].contains("components")) {
        valid_options = true;
        console->error("Components: ");
        for (auto &b: project.project_summary["choices"][i.second]["components"])
          console->error("  - {}", b.get<std::string>());
      }

      if (!valid_options) {
        console->error("ERROR: Choice data is invalid");
      }
    }
    exit(0);
  }
  if (!project.multiple_answer_choices.empty()) {
    for (auto a: project.multiple_answer_choices) {
      console->error("Choice {} - Has multiple selections", a);
    }
    exit(-1);
  }
}