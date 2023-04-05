/* A BOB workspace is identified by a .yakka folder
 * The .yakka folder holds information about locally available components, and remote registries
 */
#include "yakka.hpp"
#include "yakka_workspace.hpp"
#include "utilities.hpp"
#include <filesystem>
#include <fstream>
#include <regex>

namespace fs = std::filesystem;

std::string example_registry = "";

namespace yakka {
workspace::workspace()
{
}

void workspace::init(fs::path workspace_path)
{
  log = spdlog::get("yakkalog");

  // Load the local configuration
  this->workspace_path = workspace_path;
  load_config_file(workspace_path / "config.yaml");

  // Try determine the shared home
  if (yakka_shared_home.empty()) {
    yakka_shared_home = get_yakka_shared_home();
  }

  this->shared_components_path = yakka_shared_home;

  try {
    if (!fs::exists(shared_components_path))
      fs::create_directories(shared_components_path);
  } catch (...) {
    log->error("Failed to load shared component path");
  }

  if (!fs::exists(this->workspace_path / ".yakka/registries"))
    fs::create_directories(this->workspace_path / ".yakka/registries");

  if (!fs::exists(this->workspace_path / ".yakka/repos"))
    fs::create_directories(this->workspace_path / ".yakka/repos");

  local_database.load(this->workspace_path);

  if (!this->shared_components_path.empty()) {
    shared_database.load(this->shared_components_path);
  }

  configuration["host_os"]              = host_os_string;
  configuration["executable_extension"] = executable_extension;
  configuration_json                    = configuration.as<nlohmann::json>();
}

void workspace::load_component_registries()
{
  // Verify the .yakka/registries path exists
  if (!fs::exists(workspace_path / ".yakka/registries"))
    return;

  for (const auto &p: fs::recursive_directory_iterator(workspace_path / ".yakka/registries"))
    if (p.path().extension().generic_string() == ".yaml")
      try {
        registries[p.path().filename().replace_extension().generic_string()] = YAML::LoadFile(p.path().generic_string());
      } catch (...) {
        log->error("Could not parse component registry: '{}'", p.path().generic_string());
      }
}

yakka_status workspace::add_component_registry(const std::string &url)
{
  return fetch_registry(url);
}

std::optional<YAML::Node> workspace::find_registry_component(const std::string &name)
{
  // Look for component in registries
  for (const auto &r: registries)
    if (r.second["provides"]["components"][name].IsDefined())
      return r.second["provides"]["components"][name];
  return {};
}

std::optional<fs::path> workspace::find_component(const std::string component_dotname)
{
  bool try_update_the_database   = false;
  const std::string component_id = yakka::component_dotname_to_id(component_dotname);

  // Get component from database
  auto local  = local_database[component_id];
  auto shared = shared_database[component_id];

  // Check if that component is in the database
  if (!local && !shared)
    return {};

  if (local) {
    if (local.IsScalar()) {
      if (fs::exists(local.Scalar())) {
        return local.Scalar();
      } else {
        try_update_the_database = true;
      }
    }
    if (local.IsSequence() && local.size() == 1) {
      if (fs::exists(local[0].Scalar())) {
        return local[0].Scalar();
      } else {
        try_update_the_database = true;
      }
    }
  }

  if (shared) {
    if (shared.IsScalar() && fs::exists(shared.Scalar()))
      return shared.Scalar();
    if (shared.IsSequence() && shared.size() == 1 && fs::exists(shared[0].Scalar()))
      return shared[0].Scalar();
  }

  if (local_database.has_scanned == false && try_update_the_database == true) {
    local_database.clear();
    local_database.scan_for_components();
    return find_component(component_dotname);
  } else {
    return {};
  }
}

void workspace::load_config_file(const fs::path config_file_path)
{
  try {
    if (!fs::exists(config_file_path))
      return;

    configuration = YAML::LoadFile(config_file_path.string());

    if (configuration["path"].IsDefined()) {
      std::string path = "";
      for (const auto &p: configuration["path"]) {
        path += p.as<std::string>() + host_os_path_seperator;
      }
      path += std::getenv("PATH");
#if defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN__)
      _putenv_s("PATH", path.c_str());
#else
      setenv("PATH", path.c_str(), 1);
#endif
    }

    if (configuration["home"].IsDefined()) {
      yakka_shared_home = fs::path(configuration["home"].Scalar());
    }
  } catch (std::exception &e) {
    log->error("Couldn't read '{}'\n{}", config_file_path.string(), e.what());
  }
}

std::future<fs::path> workspace::fetch_component(const std::string &name, YAML::Node node, std::function<void(std::string, size_t)> progress_handler)
{
  std::string url                           = try_render(inja_environment, node["packages"]["default"]["url"].as<std::string>(), configuration_json, log);
  std::string branch                        = try_render(inja_environment, node["packages"]["default"]["branch"].as<std::string>(), configuration_json, log);
  const bool shared_components_write_access = (fs::status(shared_components_path).permissions() & fs::perms::owner_write) != fs::perms::none;
  fs::path git_location                     = (node["type"] && node["type"].as<std::string>() == "tool" && shared_components_write_access) ? shared_components_path / "repos" : workspace_path / ".yakka/repos";
  fs::path checkout_location                = (node["type"] && node["type"].as<std::string>() == "tool" && shared_components_write_access) ? shared_components_path / "repos" / name : workspace_path / "components" / name;
  return std::async(std::launch::async, [=]() -> fs::path {
    return do_fetch_component(name, url, branch, git_location, checkout_location, progress_handler);
  });
}

#define GIT_STRING "git"
yakka_status workspace::fetch_registry(const std::string &url)
{
  auto yakkalog                  = spdlog::get("yakkalog");
  const std::string fetch_string = "-C .yakka/registries/ clone " + url + " --progress --single-branch";
  auto [output, result]          = yakka::exec(GIT_STRING, fetch_string);

  yakkalog->info("{}", output);

  if (result != 0)
    return FAIL;

  return SUCCESS;
}

yakka_status workspace::update_component(const std::string &name)
{
  // This function could be async like fetch_component
  auto yakkalog = spdlog::get("yakkalog");
  auto console  = spdlog::get("yakkaconsole");
  std::string git_directory_string;
  if (local_database[name])
    git_directory_string = "--git-dir .yakka/repos/" + name + "/.git --work-tree components/" + name + " ";
  else if (shared_database[name])
    git_directory_string = "-C " + shared_components_path.string() + "/repos/" + name + " ";
  else
    return FAIL;

  auto [stash_output, stash_result] = yakka::exec(GIT_STRING, git_directory_string + "stash");
  if (stash_result != 0) {
    console->error(stash_output);
    return FAIL;
  }
  yakkalog->info(stash_output);

  auto [pull_output, pull_result] = yakka::exec(GIT_STRING, git_directory_string + "pull --progress");
  yakkalog->info(pull_output);

  auto [pop_output, pop_result] = yakka::exec(GIT_STRING, git_directory_string + "stash pop");
  yakkalog->info(pop_output);
  return SUCCESS;
}

using namespace std::string_literals;
fs::path workspace::do_fetch_component(const std::string &name, const std::string url, const std::string branch, const fs::path git_location, const fs::path checkout_location, std::function<void(std::string, size_t)> progress_handler)
{
  auto fetch_log = spdlog::basic_logger_mt("fetchlog-" + name, "yakka-fetch-" + name + ".log");
  fetch_log->flush_on(spdlog::level::info);

  auto yakkalog = spdlog::get("yakkalog");
  enum {
    GIT_COUNTING     = 0,
    GIT_COMPRESSING  = 1,
    GIT_RECEIVING    = 2,
    GIT_RESOLVING    = 3,
    GIT_UPDATING     = 4,
    GIT_LFS_CHECKOUT = 5,
  } phase          = GIT_COUNTING;
  int old_progress = 0;
  int retcode;

  try {
    // If the clone location already exists then something probably went wrong so delete it and it will try again
    if (!fs::exists(git_location)) {
      yakkalog->info("Creating {}", git_location.string());
      fs::create_directories(git_location);
    }

    if (!fs::exists(checkout_location)) {
      yakkalog->info("Creating {}", checkout_location.string());
      fs::create_directories(checkout_location);
    }

    if (fs::exists(git_location / name)) {
      yakkalog->info("Removing {}", (git_location / name).string());
      fs::remove_all(git_location / name);
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return {};
  }

  // Of the total time to fetch a Git repo, 10% is allocated to counting, 10% to compressing, and 80% to receiving.
  static const std::string phase_names[] = { "Counting", "Compressing", "Receiving", "Resolving", "Updating", "Fetch LFS" };
  const std::string fetch_string         = "-C \"" + git_location.string() + "\" clone " + url + " " + name + " -b " + branch + " --progress --single-branch --no-checkout";

  auto t1 = std::chrono::high_resolution_clock::now();
  retcode = yakka::exec(GIT_STRING, fetch_string, [&](std::string &data) -> void {
    fetch_log->info(data);

    std::smatch s;
    if (phase < GIT_COMPRESSING && data.find("Comp") != data.npos) {
      phase = GIT_COMPRESSING;
    }
    if (phase < GIT_RECEIVING && data.find("Rece") != data.npos) {
      phase = GIT_RECEIVING;
    }
    if (phase < GIT_RESOLVING && data.find("Reso") != data.npos) {
      phase = GIT_RESOLVING;
    }

    if (std::regex_search(data, s, std::regex{ R"(\((.*)/(.*)\))" })) {
      // yakkalog->info(data);
      int phase_progress = std::stoi(s[1]);
      int end_value      = std::stoi(s[2]);
      int progress       = (100 * phase_progress) / end_value;
      // if (progress < old_progress)
      //   yakkalog->info << name << ": " << "Progress regressed\n" << data << "\n";
      if (progress != old_progress)
        progress_handler(phase_names[phase], progress);
      old_progress = progress;
    }
  });
  if (retcode != 0) {
    return {};
  }
  auto t2       = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
  yakkalog->info("{}: cloned in {}ms", name, duration);

  const std::string checkout_string = "--git-dir \""s + git_location.string() + "/" + name + "/.git\" --work-tree \"" + checkout_location.string() + "\" checkout " + branch + " --progress --force";

  // Checkout instance
  t1      = std::chrono::high_resolution_clock::now();
  retcode = yakka::exec(GIT_STRING, checkout_string, [&](const std::string &data) -> void {
    fetch_log->info(data);

    std::smatch s;
    if (phase < GIT_UPDATING && data.find("Updat") != data.npos) {
      phase = GIT_UPDATING;
    }
    if (phase < GIT_LFS_CHECKOUT && data.find("Filt") != data.npos) {
      phase = GIT_LFS_CHECKOUT;
    }
    if (std::regex_search(data, s, std::regex{ R"(\((.*)/(.*)\))" })) {
      // yakkalog->info(data);
      int phase_progress = std::stoi(s[1]);
      int end_value      = std::stoi(s[2]);
      int progress       = (100 * phase_progress) / end_value;
      progress_handler(phase_names[phase], progress);
    }
  });
  if (retcode != 0) {
    return {};
  }
  t2       = std::chrono::high_resolution_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
  yakkalog->info("{}: checkout in {}ms", name, duration);

  progress_handler("Complete", 100);

  return checkout_location;
}

/**
     * @brief Returns the path corresponding to the home directory of BOB
     *        Typically this would be ~/.yakka or /Users/<username>/.yakka or $HOME/.yakka
     * @return std::string
     */
fs::path workspace::get_yakka_shared_home()
{
  // Try read HOME environment variable
  char *sys_home = std::getenv("HOME");
  if (sys_home != nullptr)
    return fs::path(sys_home) / ".yakka";

  // If that fails we can try the Windows version HOMEDRIVE + HOMEPATH
  char *sys_homepath  = std::getenv("HOMEPATH");
  char *sys_homedrive = std::getenv("HOMEDRIVE");
  if (sys_homepath != nullptr && sys_homedrive != nullptr)
    return fs::path(std::string(sys_homedrive) + std::string(sys_homepath)) / ".yakka";

  // Otherwise we default to using the local .yakka folder
  return ".yakka";
}

yakka_status workspace::evaluate_project(yakka::project &project)
{
}
} // namespace yakka
