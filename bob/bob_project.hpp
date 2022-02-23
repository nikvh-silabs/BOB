#pragma once

#include "bob.hpp"
#include "bob_component.hpp"
#include "component_database.hpp"
#include "blueprint_database.hpp"
#include "yaml-cpp/yaml.h"
#include "nlohmann/json.hpp"
#include "inja.hpp"
#include "spdlog/spdlog.h"
#include "indicators/progress_bar.hpp"
#include "taskflow.hpp"
#include <filesystem>
#include <regex>
#include <map>
#include <unordered_set>
#include <optional>
#include <functional>

namespace fs = std::filesystem;

namespace bob
{
    const std::string default_output_directory  = "output/";

    typedef std::function<std::string(std::string, const YAML::Node&, std::string, const nlohmann::json&, inja::Environment&)> blueprint_command;

    class project
    {
    public:
        enum class state {
            PROJECT_HAS_UNKNOWN_COMPONENTS,
            PROJECT_HAS_REMOTE_COMPONENTS,
            PROJECT_HAS_INVALID_COMPONENT,
            PROJECT_VALID
        };

    public:
        project( std::shared_ptr<spdlog::logger> log);

        virtual ~project( );

        void set_project_directory(const std::string path);
        void init_project();
        void init_project(const std::string build_string);
        void process_build_string(const std::string build_string);
        YAML::Node get_project_summary();
        void parse_project_string( const std::vector<std::string>& project_string );
        void process_requirements(YAML::Node& node, const std::string feature);
        state evaluate_dependencies();
        std::optional<fs::path> find_component(const std::string component_dotname);

        void parse_blueprints();
        void update_summary();
        void generate_project_summary();

        // Target database management
        void add_to_target_database( const std::string target );
        void generate_target_database();

        void load_common_commands();
        void set_project_file(const std::string filepath);
        void process_construction(indicators::ProgressBar& bar);
        void save_summary();
        void save_blueprints();
        std::optional<YAML::Node> find_registry_component(const std::string& name);
        std::future<void> fetch_component(const std::string& name, indicators::ProgressBar& bar);
        bool has_data_dependency_changed(std::string data_path);
        void create_tasks(const std::string target, tf::Task& parent);

        // Logging
        std::shared_ptr<spdlog::logger> log;

        // Basic project data
        std::string project_name;
        std::string output_path;
        std::string bob_home_directory;

        // Component processing
        std::set<std::string> unprocessed_components;
        std::set<std::string> unprocessed_features;
        std::unordered_set<std::string> required_features;
        std::unordered_set<std::string> commands;
        std::unordered_set<std::string> unknown_components;
        // std::map<std::string, YAML::Node> remote_components;

        YAML::Node  project_summary;
        YAML::Node  previous_summary;
        std::string project_directory;
        std::string project_summary_file;
        fs::file_time_type project_summary_last_modified;
        std::vector<std::shared_ptr<bob::component>> components;
        bob::component_database component_database;
        bob::blueprint_database blueprint_database;

        nlohmann::json project_summary_json;

        // Blueprint evaluation
        inja::Environment inja_environment;
        std::multimap<std::string, std::shared_ptr<blueprint_match> > target_database;
        std::multimap<std::string, construction_task> todo_list;
        int work_task_count;
        // YAML::Node blueprint_database;
        // std::multimap<std::string, std::shared_ptr< construction_task > > construction_list;
        // std::vector<std::string> todo_list;
        // std::map<std::string, tf::Task> tasks;
        tf::Taskflow taskflow;

        std::mutex project_lock;

        // std::vector< std::pair<std::string, YAML::Node> > blueprint_list;
        std::map< std::string, blueprint_command > blueprint_commands;

        std::function<void()> task_complete_handler;
    };

    std::string try_render(inja::Environment& env, const std::string& input, const nlohmann::json& data, std::shared_ptr<spdlog::logger> log);
    static std::pair<std::string, int> run_command( const std::string target, construction_task* task, project* project );
    static void yaml_node_merge(YAML::Node& merge_target, const YAML::Node& node);
    static void json_node_merge(nlohmann::json& merge_target, const nlohmann::json& node);
    static std::vector<std::string> parse_gcc_dependency_file(const std::string filename);
} /* namespace bob */

