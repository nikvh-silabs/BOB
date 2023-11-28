#include "slc_project.hpp"
#include "slc_sdk.hpp"
#include "cxxopts.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
//#include <indicators/progress_bar.hpp>
#include <iostream>
#include <fstream>
#include <chrono>
#include <future>
#include <filesystem>
#include <unordered_set>

using namespace std::chrono_literals;

int main(int argc, char **argv)
{
  // Setup logging
  std::error_code error_code;
  std::filesystem::remove("slc.log", error_code);
  auto slclog = spdlog::basic_logger_mt("slclog", "slc.log");

  auto console = spdlog::stdout_color_mt("console");
  console->flush_on(spdlog::level::level_enum::off);
  console->set_pattern("[%^%l%$]: %v");

  cxxopts::Options options("slc", "Silicon Labs Configurator");
  options.positional_help("<action> [optional args]");

  // clang-format off
  options.add_options()("h,help", "Print usage")
                       ("l,list", "List known components", cxxopts::value<bool>()->default_value("false"))
                       ("r,refresh", "Refresh component database", cxxopts::value<bool>()->default_value("false"))
                       ("action", "action", cxxopts::value<std::string>());
  // clang-format on

  options.parse_positional({ "action" });
  auto result = options.parse(argc, argv);
  if (result.count("help") || argc == 1) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  //slc_sdk sdk("/Users/nivonhub/SimplicityStudio/SDKs/gecko_sdk");
  slc_sdk sdk("/silabs/sl_net");
  if (result["refresh"].as<bool>()) {
    console->info("Scanning '.' for components");
    auto t1 = std::chrono::high_resolution_clock::now();
    sdk.refresh_database();
    auto t2       = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    console->info("{}ms to process components", duration);

    sdk.save_database();
    // for (const auto &i: project.slcc_database)
    //   slclog->info("{}", i.first);
  } else {
    sdk.load_database();
  }

  slc_project project;
  std::unordered_set<std::string> components = { "wifi", "brd4338a" };
  project.resolve_project(components, sdk);

  console->flush();
  return 0;
}
