#include "yakka_project.hpp"
#include "spdlog/spdlog.h"
#include "taskflow.hpp"

int main(int argc, char **argv)
{
  auto console = spdlog::stderr_color_mt("yakkaconsole");
  console->flush_on(spdlog::level::level_enum::off);
  console->set_pattern("[%^%l%$]: %v");
  auto yakkalog = spdlog::basic_logger_mt("yakkalog", "yakka_test.log");

  // Create a workspace
  // yakka::workspace workspace;

  // Create a project
  yakka::project project(yakkalog);
  project.init_project("test test!");
  project.evaluate_dependencies();
  project.generate_project_summary();
  project.save_summary();

  project.parse_blueprints();
  project.generate_target_database();

  project.load_common_commands();

  // Task flow test
  tf::Executor executor;
  auto finish = project.taskflow.emplace([=]() {
    std::cout << "FINISHED\n";
  });
  for (auto &i: project.commands)
    project.create_tasks(i, finish);

  executor.run(project.taskflow).wait();

  return 0;
}