#include "slc_sdk.hpp"
#include "yaml-cpp/yaml.h"
#include <filesystem>
#include <unordered_set>
#include <thread>
#include <vector>
#include <future>
#include <iostream>
#include <fstream>

#if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
const auto async_launch_option = std::launch::async | std::launch::deferred;
#elif defined(__APPLE__)
const auto async_launch_option = std::launch::async | std::launch::deferred;
#elif defined(__linux__)
const auto async_launch_option = std::launch::deferred;
#endif

void slc_sdk::refresh_database(const std::string path)
{
  std::vector<std::future<nlohmann::json>> parsed_slcc_files;
  auto t1 = std::chrono::high_resolution_clock::now();
  for (const auto &p: std::filesystem::recursive_directory_iterator(path)) {
    if (p.path().filename().extension() == ".slcc") {
      parsed_slcc_files.push_back(std::async(
        async_launch_option,
        [](std::string path) -> nlohmann::json {
          try {
            return YAML::LoadFile(path).as<nlohmann::json>();
          } catch (std::exception &e) {
            std::cerr << "Bad YAML: " << path << "\r\n" << e.what() << "\r\n";
            return {};
          }
        },
        p.path().generic_string()));
    }
  }

  auto t2       = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
  std::cerr << duration << "ms to find files\n";

  for (auto &i: parsed_slcc_files) {
    try {
      nlohmann::json result = i.get();
      if (!result.is_null() && result.contains("id")) {
        // Add component to the database
        const auto id              = result["id"].get<std::string>();
        database["components"][id] = result;

        // Add the component 'provides' into the universal multimap to help resolve dependencies
        if (result.contains("provides"))
          for (const auto &p: result["provides"])
            database["provides"][p["name"].get<std::string>()].push_back(id);
      }
    } catch (std::exception &e) {
      std::cerr << e.what() << std::endl;
    }
  }
}

void slc_sdk::load_database()
{
  std::ifstream f(root_path / "database.json");
  database = nlohmann::json::parse(f);
}

void slc_sdk::save_database()
{
  std::ofstream json_file(root_path / "database.json");
  json_file << database.dump(3);
  json_file.close();
}