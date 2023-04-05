#include "slc_project.h"
#include <filesystem>
#include <unordered_set>
#include <thread>
#include <vector>
#include <future>

/*
1. We start with a list of components (C).
2. These components all have requirements and provides. These are aggregated into a set of requirements (R) and a set of provides (P) for the project.
3. The set of unsatisfied requirements is the set of requirements minus the set of provides (UR=R-P).
4. Loop over the set of unsatisfied requirements (UR):
   a. Find all components that provide the unsatisfied requirement
      i. If all conditions on a provide is in the combined set of provides and requirements (R+P), the new provide is considered.
        (This means that all requirements are implicitly considered as provided when we look for provides. The reason for this is that they will be provided at some point, thereby including the provide, or the project resolution will fail.)
   b. If only one new component was found: it is added to the list of components for the project (C).
5. If we added at least one new component to the list of components during step 4 we jump back to step 1 and start again.
6. We have reached steady-state, and inspect the results.
   a. If the set of requirements is covered by the set of provides, and no two components give the same provide, the project has been resolved successfully.
   b. If not the project resolution failed
*/

void slc_project::resolve_project(std::unordered_set<std::string> components)
{
  std::vector<std::string> unsatisified_requirements;
  std::unordered_set<std::string>
  requires;
  std::unordered_set<std::string> provides;

  // Get the list of unsatisified requirements
  std::set_difference(requires.begin(), requires.end(), provides.begin(), provides.end(), std::inserter(unsatisified_requirements, unsatisified_requirements.begin()));

  for (const auto &r: unsatisified_requirements) {
    // Get all the components that provide the requirement
    auto p = provided_requirements.equal_range(r);
    for (auto i = p.first; i != p.second; ++i) {
      // Check conditions of
    }
  }
}

void slc_project::generate_slcc_database(const std::string path)
{
  std::vector<std::future<YAML::Node>> parsed_slcc_files;
  for (const auto &p: std::filesystem::recursive_directory_iterator(path)) {
    if (p.path().filename().extension() == ".slcc") {
      parsed_slcc_files.push_back(std::async(
        std::launch::async,
        [](std::string path) -> YAML::Node {
          try {
            return YAML::LoadFile(path);
          } catch (...) {
            return {};
          }
        },
        p.path().generic_string()));
    }
  }

  for (const auto &i: parsed_slcc_files) {
    YAML::Node result = i.wait().value();
    if (!result.IsNull()) {
      // Add component to the database
      slcc_database[result["name"].as<std::string>()] = result;

      // Extra the 'provides' into the universal multimap
      for (const auto &p: result["provides"])
        provided_requirements.insert({ p.as<std::string>(), result });
    }
  }
}