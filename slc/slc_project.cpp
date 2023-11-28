#include "slc_project.hpp"
#include "spdlog.h"
#include <filesystem>
#include <unordered_set>
#include <thread>
#include <vector>
#include <future>
#include <iostream>

#if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
const auto async_launch_option = std::launch::async | std::launch::deferred;
#elif defined(__APPLE__)
const auto async_launch_option = std::launch::async | std::launch::deferred;
#elif defined(__linux__)
const auto async_launch_option = std::launch::deferred;
#endif

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

void slc_project::resolve_project(std::unordered_set<std::string> components, const slc_sdk &sdk)
{
  std::unordered_set<std::string> unsatisified_requirements;
  std::unordered_set<std::string> required;
  std::unordered_set<std::string> recommended;
  std::unordered_set<std::string> provides;

  this->components = components;

  // Find each component in SDK and create list of requires and provides
  for (const auto &c: this->components) {
    if (sdk.database["components"].contains(c)) {
      const auto component = sdk.database["components"][c];
      if (component.contains("requires"))
        for (const auto &r: component["requires"])
          required.insert(r["name"].get<std::string>());
      if (component.contains("recommends"))
        for (const auto &r: component["recommends"])
          recommended.insert(r["id"].get<std::string>());
    } else {
      spdlog::error("Can't find component");
    }
  }

  // Get the list of unsatisified requirements
  std::set_difference(required.begin(), required.end(), provides.begin(), provides.end(), std::inserter(unsatisified_requirements, unsatisified_requirements.begin()));

  for (const auto &r: unsatisified_requirements) {
    // Get all the components that provide the requirement
    if (sdk.database["provides"].contains(r)) {
      const auto list_of_options = sdk.database["provides"][r];
      for (const auto &o: list_of_options) {
        const auto id = o.get<std::string>();
        if (recommended.contains(id) || list_of_options.size() == 1) {
          spdlog::error("Found a recommendation!");
          this->components.insert(id);
        }
      }
      //auto p = provided_requirements.equal_range(r);
      // for (auto i = p.first; i != p.second; ++i) {
      // Check conditions
      //   if (check_conditions(i->second["provides"][i->first]["conditions"])) {
      //     // This component satisifies the requirement. Add to the list
      //   }
      // }
    }
  }
}
