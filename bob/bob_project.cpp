#include "bob_project.h"
#include "spdlog/spdlog.h"
#include <fstream>
#include <chrono>
#include <thread>

namespace bob
{
    using namespace std::chrono_literals;

    project::project( std::shared_ptr<spdlog::logger> log ) : project_directory("."), bob_home_directory("/.bob")
    {
        this->log = log;
        load_config_file("config.yaml");
        project_summary["configuration"]["host_os"] = host_os_string;
        project_summary["configuration"]["executable_extension"] = executable_extension;
    }

    project::~project( )
    {
    }


    project::project( const std::vector<std::string>& project_string, std::shared_ptr<spdlog::logger> log ) : project_directory("."), bob_home_directory("/.bob")
    {
        this->log = log;
        load_config_file("config.yaml");
        project_summary["configuration"]["host_os"] = host_os_string;
        project_summary["configuration"]["executable_extension"] = executable_extension;
        parse_project_string(project_string);
    }

    void project::set_project_directory(const std::string path)
    {
        project_directory = path;
    }


    YAML::Node project::get_project_summary()
    {
        return project_summary;
    }

    void project::parse_project_string( const std::vector<std::string>& project_string )
    {
        project_name = "";

        // First string is interpreted as a command
        auto s = project_string.begin();
        commands.insert(*s++);
        for ( ; s != project_string.end(); ++s)
        {
            // Identify features, commands, and components
            if ( s->front( ) == '+' )
                unprocessed_features.push_back( s->substr(1) );
            else if ( s->back( ) == '!' )
                commands.insert( s->substr(0, s->size()-1) );
            else
                unprocessed_components.push_back( *s );
        }

        // Generate the project name from the project string
        for ( const auto& i : unprocessed_components )
            project_name += i + "-";

        if (!unprocessed_components.empty())
            project_name.pop_back( );

        for ( const auto& i : unprocessed_features )
            project_name += "-" + i;

        if (project_name.empty())
            project_name = "none";

        // Add standard information into the project summary
        project_summary["project_name"]   = project_name;
        project_summary["project_output"] = default_output_directory + project_name;
    }

    void project::process_requirements(const YAML::Node& node)
    {
        if (node.IsScalar () || node.IsSequence ())
        {
            log->error("Node 'requires' entry is malformed: '{}'", node.Scalar());
            return;
        }

        try
        {
            // Process required components
            if (node["components"])
            {
                // Add the item/s to the new_component list
                if (node["components"].IsScalar ())
                    unprocessed_components.push_back (node["components"].as<std::string> ());
                else if (node["components"].IsSequence ())
                    for (auto &i : node["components"])
                        unprocessed_components.push_back (i.as<std::string> ());
                else
                    log->error("Node '{}' has invalid 'requires'", node.Scalar());
            }

            // Process required features
            if (node["features"])
            {
                std::vector<std::string> new_features;

                // Add the item/s to the new_features list
                if (node["features"].IsScalar ())
                    unprocessed_features.push_back (node["features"].as<std::string> ());
                else if (node["features"].IsSequence ())
                    for (auto &i : node["features"])
                        unprocessed_features.push_back (i.as<std::string> ());
                else
                    log->error("Node '{}' has invalid 'requires'", node.Scalar());
            }
        }
        catch (YAML::Exception &e)
        {
            log->error("Failed to process requirements for '{}'\n{}", node.Scalar(), e.msg);
        }
    }

    void project::process_supported_feature(YAML::Node& component, const YAML::Node& node)
    {
        // Process all the "requires"
        if (node["requires"])
        {
            process_requirements (node["requires"]);
        }

        // Merge all nodes of this feature to the parent component
//        bob::yaml_node_merge(component, node);
    }


    /**
     * @brief Processes all the @ref unprocessed_components and @ref unprocessed_features, adding items to @ref unknown_components if they are not in the component database
     *        It is assumed the caller will process the @ref unknown_components before adding them back to @ref unprocessed_component and calling this again.
     * @return project::state
     */
    project::state project::evaluate_dependencies()
    {
        // Start processing all the required components and features
        while ( !unprocessed_components.empty( ) || !unprocessed_features.empty( ))
        {
            // Loop through the list of unprocessed components.
            // Note: Items will be added to unprocessed_components during processing
            for ( auto i = 0; i < unprocessed_components.size(); ++i )
            {
                // Convert string to id
                const auto c = bob::component_dotname_to_id(unprocessed_components[i]);

                // Find the component in the project component database
                auto component_path = find_component(c);
                if ( !component_path )
                {
                    // log->info("{}: Couldn't find it", c);
                    unknown_components.insert(c);
                    continue;
                }

                // Add component to the required list and continue if this is not a new component
                if ( required_components.insert( c ).second == false )
                {
                    log->info("{}: Unprocessed component already required", c);
                    continue;
                }

                std::shared_ptr<bob::component> new_component;
                try
                {
                    new_component = std::make_shared<bob::component>( component_path.value() );
                    components.push_back( new_component );
                }
                catch ( std::exception e )
                {
                    log->error("Failed to parse: {}\n{}", c, e.what());
                    required_components.erase(c);
                    //unknown_components.insert(c);
                }

                // Add all the required components into the unprocessed list
                for (const auto& r : new_component->yaml["requires"]["components"])
                    unprocessed_components.push_back(r.Scalar());

                // Add all the required features into the unprocessed list
                for (const auto& f : new_component->yaml["requires"]["features"])
                    unprocessed_features.push_back(f.Scalar());

                // Process all the currently required features. Note new feature will be processed in the features pass
                for ( auto& f : required_features )
                    if ( new_component->yaml["supports"][f] )
                        process_supported_feature( new_component->yaml, new_component->yaml["supports"][f] );
            }
            unprocessed_components.clear();

            // Process all the new features
            // Note: Items will be added to unprocessed_features during processing
            for ( auto i = 0; i < unprocessed_features.size(); ++i )
            {
                const auto f = unprocessed_features[i];

                // Insert feature and continue if this is not a new feature
                if ( required_features.insert( f ).second == false )
                    continue;

                // Update each component with the new feature
                for ( auto& c : components )
                    if ( c->yaml["supports"][f] )
                        process_supported_feature( c->yaml, c->yaml["supports"][f] );
            }
            unprocessed_features.clear();
        };

        if (unknown_components.size() != 0) return project::state::PROJECT_HAS_UNKNOWN_COMPONENTS;

        return project::state::PROJECT_VALID;
    }

    void project::generate_project_summary()
    {
        if (!project_summary["tools"])
          project_summary["tools"] = YAML::Node();

        // Put all YAML nodes into the summary
        for (const auto& c: components)
        {
            project_summary["components"][c->id] = c->yaml;
            for (auto tool: c->yaml["tools"])
            {
                inja::Environment inja_env = inja::Environment();
                inja_env.add_callback("curdir", 0, [&c](const inja::Arguments& args) { return c->yaml["directory"].Scalar();});

                project_summary["tools"][tool.first.Scalar()] = inja_env.render(tool.second.Scalar(), configuration_json);
            }
        }

        // Process all the supported features by merging their content with the parent component
        for ( auto c : project_summary["components"] )
        	if (c.second["supports"].IsDefined())
				for ( auto f : this->required_features )
					if ( c.second["supports"][f].IsDefined() )
						yaml_node_merge( c.second, c.second["supports"][f] );


        project_summary["features"] = {};
        for (const auto& i: this->required_features)
        	project_summary["features"].push_back(i);

        project_summary_json = project_summary.as<nlohmann::json>();
        project_summary_json["data"] = nlohmann::json::object();
        project_summary_json["host"] = nlohmann::json::object();
        project_summary_json["host"]["name"] = host_os_string;
    }

    std::optional<fs::path> project::find_component(const std::string component_dotname)
    {
        const std::string component_id = bob::component_dotname_to_id(component_dotname);

        // Get component from database
        const auto& c = component_database[component_id];

        // Check if that component is in the database
        if (!c)
            return {};

        if (c.IsScalar() && fs::exists(c.Scalar()))
            return c.Scalar();
        if (c.IsSequence())
        {
            if ( c.size( ) == 1 )
            {
                if ( fs::exists( c[0].Scalar( ) ) )
                    return c[0].Scalar( );
            }
            else
                log->error("TODO: Parse multiple matches to the same component ID: '{}'", component_id);
        }
        return {};
    }

    void project::parse_blueprints()
    {
        for ( auto c: project_summary["components"])
            for ( auto b: c.second["blueprints"] )
            {
                std::string blueprint_string = inja_environment.render( b.second["regex"] ? b.second["regex"].Scalar() : b.first.Scalar( ), project_summary_json );
                log->info("Blueprint: {}", blueprint_string);
                b.second["bob_parent_path"] = c.second["directory"];
                blueprint_list.insert( blueprint_list.end( ), { blueprint_string, b.second } );
            }
    }

    void project::evaluate_blueprint_dependencies()
    {
        std::unordered_set<std::string> new_targets;
        std::unordered_set<std::string> processed_targets;
        std::unordered_set<std::string> unprocessed_targets = commands;

        while (!unprocessed_targets.empty())
        {
            for (auto& t: unprocessed_targets)
            {
                // Add to processed targets and check if it's already been processed
                if (processed_targets.insert(t).second == false)
                    continue;

                auto matches = find_blueprint_match(t);
                for (auto& match: matches)
                {
                    new_targets.insert(match->dependencies.begin(), match->dependencies.end());
                    auto new_task = std::make_shared<construction_task>();
                    new_task->blueprint = std::move(match);
                    new_task->state = bob_task_to_be_done;
                    construction_list.insert( std::make_pair(t, new_task ));
                }
                todo_list.push_back(t);
            }

            unprocessed_targets.clear();
            unprocessed_targets.swap(new_targets);
        }
    }

    void project::process_data_dependency(const std::string& path)
    {
        if (path.front() != '/')
        {
            // TODO: Throw exception
            log->error( "Data path does not start with '/'");
            return;
        }

        for (const auto& c: project_summary_json["components"])
        {
            try {
                json_node_merge(project_summary_json[nlohmann::json::json_pointer("/data" + path)], c.at(nlohmann::json::json_pointer(path)));
            }
            catch (...) {}
        }
    }

    std::vector<std::unique_ptr<blueprint_match>> project::find_blueprint_match( const std::string target )
    {
        std::vector<std::unique_ptr<blueprint_match>> blueprint_matches;

        for ( auto& b : blueprint_list )
        {
            std::smatch s;

            // Check if rule is a regex, otherwise do a string comparison
            if ( b.second["regex"] )
            {
                if ( !std::regex_match(target, s, std::regex { b.first } ) )
                    continue;
            }
            else
            {
                if (target != b.first )
                    continue;
            }

            auto match = std::make_unique<blueprint_match>();
            match->target = target;
            match->blueprint = b.second;

            // Process match
            if ( b.second["regex"] )
            {
                // arg_count starts at 0 as the first match is the entire string
                int arg_count = 0;
                for ( auto& regex_match : s )
                {
                    match->regex_matches[arg_count] = regex_match.str( );
                    ++arg_count;
                }
            }
            else
            {
                match->regex_matches[0] = target;
            }

            inja::Environment local_inja_env;
            local_inja_env.add_callback("$", 1, [&match](const inja::Arguments& args) {
                        return match->regex_matches[ args[0]->get<int>() ];
                    });

            local_inja_env.add_callback("curdir", 0, [&match](const inja::Arguments& args) { return match->blueprint["bob_parent_path"].Scalar();});

            // Run template engine on dependencies
            for ( auto d : b.second["depends"] )
            {
                // Check for special dependency_file condition
                if ( d.IsMap( ) )
                {
                    if ( d.begin()->first.Scalar() == "dependency_file" )
                    {
                        const std::string generated_dependency_file = local_inja_env.render( d.begin()->second.Scalar(), project_summary_json );
                        auto dependencies = parse_gcc_dependency_file(generated_dependency_file);
                        match->dependencies.insert( std::end( match->dependencies ), std::begin( dependencies ), std::end( dependencies ) );
                        continue;
                    }
                    else if (d.begin()->first.Scalar().compare( "data" ) == 0)
                    {
                        const std::string data_path = local_inja_env.render( d.begin()->second.Scalar(), project_summary_json );
                        required_data.insert(data_path);
                        process_data_dependency(data_path);
                    }
                }
                // Verify validity of dependency
                else if ( !d.IsScalar( ) )
                {
                    log->error("Dependencies only support Scalar entries");
                    return {};
                }

                std::string depend_string = d.Scalar( );

                // Generate full dependency string by applying template engine
                std::string generated_depend;
                try
                {
                    generated_depend = local_inja_env.render( depend_string, project_summary_json );
                }
                catch ( std::exception& e )
                {
                    log->error("Couldn't apply template: '{}'", depend_string);
                    return {};
                }

                // Check if the input was a YAML array construct
                if ( depend_string.front( ) == '[' && depend_string.back( ) == ']' )
                {
                    // Load the generated dependency string as YAML and push each item individually
                    YAML::Node generated_node = YAML::Load( generated_depend );
                    for ( auto i : generated_node )
                        match->dependencies.push_back( i.Scalar( ) );
                }
                else
                {
                    match->dependencies.push_back( generated_depend );
                }
            }

            // If the file exists, get the last modified timestamp
            if ( fs::exists( target ) )
                match->last_modified = fs::last_write_time( target );

            blueprint_matches.push_back( std::move( match ) );
        }

        if (blueprint_matches.empty())
        {
            if (fs::exists( target ))
            {
                auto match = std::make_unique<blueprint_match>();
                match->target        = target;
                match->blueprint     = YAML::Node();
                match->last_modified = fs::last_write_time( target );
                blueprint_matches.push_back( std::move( match ) );
            }
            else
                log->info("No blueprint for '{}'", target);
        }

        return blueprint_matches;
    }

    void project::load_common_commands()
    {
        blueprint_commands["echo"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            if (!command.begin()->second.IsNull())
                captured_output = inja_env.render(command.begin()->second.as<std::string>(), generated_json);

            auto console = spdlog::get("bobconsole");
            console->info("{}", captured_output);
            return captured_output;
        };


        blueprint_commands["execute"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            if (command.begin()->second.IsNull())
                return "";
            auto boblog = spdlog::get("boblog");
            auto console = spdlog::get("bobconsole");
            std::string temp = command.begin( )->second.as<std::string>( );
            try {
                captured_output = inja_env.render( temp, generated_json );
                //std::replace( captured_output.begin( ), captured_output.end( ), '/', '\\' );
                boblog->info("Executing '{}'", captured_output);
                auto [temp_output, retcode] = exec( captured_output, std::string( "" ) );

                if (retcode < 0 && temp_output.length( ) != 0) {
                    console->error( temp_output );
                    boblog->error( "\n{} returned {}\n{}", captured_output, retcode, temp_output);
                }
                else if ( temp_output.length( ) != 0 )
                    boblog->info("{}", temp_output);
                return temp_output;
            }
            catch (std::exception& e)
            {
                boblog->error( "Failed to execute: {}", temp);
                captured_output = "";
                return "";
            }
        };

        blueprint_commands["fix_slashes"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            std::replace(captured_output.begin(), captured_output.end(), '\\', '/');
            return captured_output;
        };


        blueprint_commands["regex"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            std::regex regex_search(command["search"].as<std::string>());
            if (command["split"])
            {
                std::istringstream ss(captured_output);
                std::string line;
                captured_output = "";
                int count = 0;

                while (std::getline(ss, line))
                {
                  if (command["replace"])
                  {
                      std::string r = std::regex_replace(line, regex_search, command["replace"].as<std::string>(), std::regex_constants::format_no_copy);
                      captured_output.append(r);
                  }
                  else if (command["to_yaml"])
                  {
                    std::smatch s;
                    if (!std::regex_match(line, s, regex_search))
                      continue;
                    YAML::Node node;
                    node[0] = YAML::Node();
                    int i=1;
                    for ( auto& v : command["to_yaml"] )
                      node[0][v.Scalar()] = s[i++].str();

                    captured_output.append(YAML::Dump(node));
                    captured_output.append("\n");
                  }
                }
            }
            else
            {
                captured_output = std::regex_replace(captured_output, regex_search, command["replace"].as<std::string>());
            }
            return captured_output;
        };


        blueprint_commands["inja"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            auto boblog = spdlog::get("boblog");
            try
            {
                if (command["file"])
                {
                    std::string filename = inja_env.render(command["file"].as<std::string>(), generated_json);

                    if (!fs::exists(filename))
                        boblog->error( "{} not found when trying to apply template engine", filename);
                    else
                        captured_output = inja_env.render_file(filename, generated_json);
                }
                else
                {
                    const auto& node = (command["template"]) ? command["template"] : command["inja"];
                    captured_output = inja_env.render(node.Scalar(), generated_json);
                }
            }
            catch (std::exception &e)
            {
                boblog->error("Failed to apply template: {}\n{}", command.Scalar(), e.what());
            }
            return captured_output;
        };

        blueprint_commands["save"] = []( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env) -> std::string {
            std::string save_filename;
            auto boblog = spdlog::get("boblog");

            if (command.begin()->second.IsNull())
                save_filename = target;
            else
                save_filename = inja_env.render(command.begin()->second.as<std::string>(), generated_json);

            try
            {
                std::ofstream save_file;
                fs::path p(save_filename);
                if (!p.parent_path().empty())
                  fs::create_directories(p.parent_path());
                save_file.open(save_filename);
                save_file << captured_output;
                save_file.close();
            }
            catch (std::exception& e)
            {
                boblog->error("Failed to save file: '{}'", save_filename);
                captured_output = "";
            }
            return captured_output;
        };

        blueprint_commands["create_directory"] = [ ]( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env ) -> std::string {
            auto boblog = spdlog::get("boblog");
            if (!command.begin()->second.IsNull())
            {
                std::string filename = "";
                try
                {
                    filename = command.begin()->second.as<std::string>( );
                    filename = inja_env.render(filename, generated_json);
                    if ( !filename.empty( ) )
                    {
                        fs::path p( filename );
                        fs::create_directories( p.parent_path( ) );
                    }
                }
                catch ( std::exception e )
                {
                    boblog->error( "Couldn't create directory for '{}'", filename);
                }
            }
            return "";
        };

        blueprint_commands["verify"] = [ ]( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env ) -> std::string {
            std::string filename = command.begin()->second.as<std::string>( );
            filename = inja_env.render(filename, generated_json);
            auto boblog = spdlog::get("boblog");
            if (fs::exists(filename))
                boblog->info("{} exists", filename);
            else
                boblog->info("BAD!! {} doesn't exist", filename);
            return captured_output;
        };

        blueprint_commands["rm"] = [ ]( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env ) -> std::string {
            std::string filename = command.begin()->second.as<std::string>( );
            filename = inja_env.render(filename, generated_json);
            fs::remove(filename);
            return captured_output;
        };

        blueprint_commands["pack"] = [ ]( std::string target, const YAML::Node& command, std::string captured_output, const nlohmann::json& generated_json, inja::Environment& inja_env ) -> std::string {
            std::string format = command["format"].as<std::string>();
            format = inja_env.render(format, generated_json);

            std::string pack_arguments = "-ie pack(\"" + format + "\"";

            for (const auto& v: command["values"])
              pack_arguments += ", " + inja_env.render(v.as<std::string>(), generated_json);

            // auto output, ret = bob::exec("perl", pack_arguments);

            return captured_output;
        };
    }

    static std::pair<std::string, int> run_command( std::shared_ptr< construction_task> task, const project* project )
    {
        auto boblog = spdlog::get("boblog");
        std::string captured_output;
        inja::Environment inja_env = inja::Environment();
        auto& blueprint = task->blueprint;

        inja_env.add_callback("$", 1, [&blueprint](const inja::Arguments& args) {
            return blueprint->regex_matches[ args[0]->get<int>() ];
        });

        inja_env.add_callback("curdir", 0, [&blueprint](const inja::Arguments& args) { return blueprint->blueprint["bob_parent_path"].Scalar();});
        inja_env.add_callback("filesize", 1, [&blueprint](const inja::Arguments& args) { return fs::file_size(args[0]->get<std::string>());});


        if ( !blueprint->blueprint["process"].IsSequence())
        {
            boblog->error("Error: process nodes must be sequences: {}", blueprint->blueprint["process"].Scalar());
            return {"", -1};
        }

        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();

        // Note: A blueprint process is a sequence of maps
        for ( const auto command_entry : blueprint->blueprint["process"] )
        {
            // Take the first entry in the map as the command
            auto        command      = command_entry.begin();
            std::string command_name = command->first.as<std::string>();

            try
            {
                // Verify tool exists
                if (project->project_summary["tools"][command_name].IsDefined())
                {
                    YAML::Node tool = project->project_summary["tools"][command_name];
                    std::string command_text = "";

//                    if ( tool["prefix"] )
//                        command_text.append( tool["prefix"].Scalar() );

                    command_text.append( tool.as<std::string>( ) );

                    std::string arg_text = command->second.as<std::string>( );

                    // Apply template engine
                    arg_text = inja_env.render( arg_text, project->project_summary_json);

                    auto[temp_output, retcode] = exec(command_text, arg_text);

                    if (retcode < 0)
                    {
                      boblog->error(temp_output);
                      return {temp_output, retcode};
                    }
                    captured_output = temp_output;
                    // Echo the output of the command
                    // TODO: Note this should be done by the main thread to ensure the outputs from multiple run_command instances don't overlap
                    boblog->info(captured_output);
                }
                else if (project->blueprint_commands.at(command_name))
                {
                    captured_output = project->blueprint_commands.at(command_name)( blueprint->target, command_entry, captured_output, project->project_summary_json, inja_env );
                }
                else
                {
                    boblog->error("{} tool doesn't exist", command_name);
                }

            }
            catch ( std::exception& e )
            {
                boblog->error("Failed to run command: '{}' as part of {}", command_name, blueprint->target);
                boblog->info( "Failed to run: {}", command_entry.Scalar());
                throw e;
            }
        }

        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        boblog->info( "{}: {} milliseconds", blueprint->target, duration);
        return {captured_output, 0};
    }

    static void json_node_merge(nlohmann::json& merge_target, const nlohmann::json& node)
    {
        auto boblog = spdlog::get("boblog");
        switch(node.type())
        {
            case nlohmann::detail::value_t::object:
                switch(merge_target.type())
                {
                    case nlohmann::detail::value_t::object:
                    case nlohmann::detail::value_t::array:
                    default:
                        boblog->error("Currently not supported"); break;
                }
                break;
            case nlohmann::detail::value_t::array:
                switch(merge_target.type())
                {
                    case nlohmann::detail::value_t::object:
                        boblog->error("Cannot merge array into an object"); break;
                    case nlohmann::detail::value_t::array:
                        for (auto& i: node)
                            merge_target.push_back(i);
                        break;
                    default:
                        merge_target.push_back(node); break;
                }
                break;
            default:
                switch(merge_target.type())
                {
                    case nlohmann::detail::value_t::object:
                        boblog->error("Cannot merge scalar into an object"); break;
                    case nlohmann::detail::value_t::array:
                    default:
                        merge_target.push_back(node); break;
                }
                break;
        }
    }

    static void yaml_node_merge(YAML::Node& merge_target, const YAML::Node& node)
    {
        auto boblog = spdlog::get("boblog");
        for (const auto& i : node)
        {
            const std::string item_name = i.first.as<std::string>();
            YAML::Node        item_node = i.second;

            if ( !merge_target[item_name] )
            {
                merge_target[item_name] = item_node;
            }
            else
            {
                if (item_node.IsScalar())
                {
                    if (merge_target[item_name].IsScalar())
                    {
                        YAML::Node new_node;
                        new_node.push_back(merge_target[item_name]);
                        new_node.push_back(item_node.Scalar());
                        merge_target[item_name].reset(new_node);
                    }
                    else if (merge_target[item_name].IsSequence())
                    {
                        merge_target[item_name].push_back(item_node.Scalar());
                    }
                    else
                    {
                        boblog->error("Cannot merge scalar and map\nScalar: {}\nMap: {}", i.first.as<std::string>(), merge_target[item_name].as<std::string>());
                        return;
                    }
                }
                else if (item_node.IsSequence())
                {
                    if (merge_target[item_name].IsMap())
                    {
                        boblog->error("Cannot merge sequence and map\n{}\n{}", merge_target.as<std::string>(), node.as<std::string>());
                        return;
                    }
                    if (merge_target[item_name].IsScalar())
                    {
                        // Convert merge_target from a scalar to a sequence
                        YAML::Node new_node;
                        new_node.push_back( merge_target[item_name].Scalar() );
                        merge_target[item_name] = new_node;
                    }
                    for (auto a : item_node)
                    {
                        merge_target[item_name].push_back(a);
                    }
                }
                else if (item_node.IsMap())
                {
                    if (!merge_target[item_name].IsMap())
                    {
                        boblog->error("Cannot merge map and non-map\n{}\n{}", merge_target.as<std::string>(), node.as<std::string>());
                        return;
                    }
                    auto new_merge = merge_target[item_name];
                    yaml_node_merge(new_merge, item_node);
                }
            }
        }
    }

    void project::process_construction(indicators::ProgressBar& bar)
    {
        auto boblog = spdlog::get("boblog");
        typedef enum
        {
            nothing_to_do,
            dependency_not_ready,
            set_to_complete,
            execute_process,
        } blueprint_status;
        int loop_count = 0;
        bool something_updated = true;
        auto construction_start_time = fs::file_time_type::clock::now();
        int starting_size = todo_list.size( );
        int completed_tasks = 0;

        if ( todo_list.size( ) == 0 )
            return;

        std::vector<std::shared_ptr<construction_task>> running_tasks;

        // Process all the stuff to be built
        boblog->info("------ Building ------");
        boblog->info("{} items left to construct", todo_list.size());

    #if defined(CONSTRUCTION_LIST_DUMP)
        for (auto a: construction_list) std::cout << "- " << a.first << "\n";
    #endif

        // loop through list
        int loop = 0;
        int last_progress_update = 0;
        auto i = todo_list.begin();
        while ( todo_list.size() != 0 )
        {
            ++loop;

            while (running_tasks.size() >= std::thread::hardware_concurrency())
            {
                // Update the modified times of the construction items
                for (auto a = running_tasks.begin(); a != running_tasks.end();)
                {
                    if ( a->get()->thread_result.wait_for(2ms) == std::future_status::ready )
                    {
                        boblog->info( "{}: Done", a->get()->blueprint->target);
                        a->get()->blueprint->last_modified = construction_start_time;
                        a->get()->state = bob_task_complete;
                        a = running_tasks.erase( a );
                        ++completed_tasks;
                    }
                    else
                        ++a;
                }
            }
            // } while ;

            int progress = (100 * completed_tasks)/starting_size;
            if (progress != last_progress_update)
            {
                bar.set_progress(progress);
                last_progress_update = progress;
            }

            // Check if we've done a pass through all the items in the construction list
            if ( i == todo_list.end( ) )
            {
                if (running_tasks.size() == 0 && something_updated == false )
                    break;

                // Update the modified times of the construction items
                for (auto a = running_tasks.begin(); a != running_tasks.end();)
                {
                    if ( a->get()->thread_result.wait_for(0ms) == std::future_status::ready )
                    {
                        boblog->info( "{}: Done", a->get()->blueprint->target);
                        a->get()->blueprint->last_modified = construction_start_time;
                        a->get()->state = bob_task_complete;
                        a = running_tasks.erase( a );
                        ++completed_tasks;
                    }
                    else
                        ++a;
                }

                // Reset iterator back to the start of the list and continue
                i = todo_list.begin( );
                something_updated = false;
            }

            auto task_list = construction_list.equal_range(*i);

            // Check validity of task_list
            if ( task_list.first == construction_list.cend( ) )
            {
                boblog->info("Couldn't find '{}' in construction list", *i);
                i = todo_list.erase(i);
                continue;
            }

            auto is_task_complete = [task_list]() -> bool {
                for(auto i = task_list.first; i != task_list.second; ++i)
                    if (i->second->state != bob_task_complete)
                        return false;
                return true;
            };

            auto get_task_status = [this](construction_task& task) -> blueprint_status {
                blueprint_status status = task.blueprint->last_modified.time_since_epoch().count() == 0 ? execute_process : set_to_complete;
                for ( const auto& d: task.blueprint->dependencies )
                {
                    for (auto [start,end] = this->construction_list.equal_range(d); start != end; ++start)
                    {
                        if ( start->second->state != bob_task_complete)
                            return dependency_not_ready;
                        if ( (start->second->blueprint->last_modified > task.blueprint->last_modified ) && ( task.blueprint->blueprint["process"]) )
                            status = execute_process;
                    }
                }
                return status;
            };

            // Check if the target needs, and is ready for, processing
            if (is_task_complete())
            {
                something_updated = true;
//                std::cout << "Finished with: " << *i << std::endl;
                i = todo_list.erase(i);
                continue;
            }

            for (auto t=task_list.first; t != task_list.second; ++t)
            {
                if (t->second->state == bob_task_to_be_done)
                    switch ( get_task_status( *(t->second) ) )
                    {
                        case set_to_complete:
                            t->second->state = bob_task_complete;
                            if (starting_size > 1) --starting_size;
                            something_updated = true;
                            break;
                        case execute_process:
                            something_updated = true;
                            if ( t->second->blueprint->blueprint["process"])
                            {
                                boblog->info( "{}: Executing blueprint", t->second->blueprint->target);
                                t->second->thread_result = std::async(std::launch::async | std::launch::deferred, run_command, t->second, this);
                                running_tasks.push_back(t->second);
                                t->second->state = bob_task_executing;
                            }
                            else
                                t->second->state = bob_task_complete;
                            break;
                        default:
                        	break;
                    }
            }

            i++;
        }

        bar.set_progress(100);
        // bar.mark_as_completed();

        for (auto& a: todo_list )
        {
            boblog->info( "Couldn't build: {}", a);
            for (auto entries = this->construction_list.equal_range(a); entries.first != entries.second; ++entries.first)
                for (auto b: entries.first->second->blueprint->dependencies)
                    boblog->info( "\t{}", b);
        }

        for (auto a: construction_list)
            if (a.second->state == bob_task_to_be_done )
                boblog->info("{}", a.first);
    }

    void project::load_config_file(const std::string config_filename)
    {
        if (!fs::exists(config_filename))
            return;

        try
        {
            auto configuration = YAML::LoadFile( config_filename );

            project_summary["configuration"] = configuration;
            project_summary["tools"] = configuration["tools"];

            if (configuration["bob_home"].IsDefined())
            {
                bob_home_directory =  configuration["bob_home"].Scalar();
                if (!fs::exists(bob_home_directory + "/repos"))
                    fs::create_directories(bob_home_directory + "/repos");
            }

            configuration_json = configuration.as<nlohmann::json>();
        }
        catch ( std::exception &e )
        {
            log->error("Couldn't read '{}'\n{}", config_filename, e.what( ));
            project_summary["configuration"];
            project_summary["tools"] = "";
        }

    }

    /**
     * @brief Save to disk the content of the @ref project_summary to bob_summary.yaml and bob_summary.json
     *
     */
    void project::save_summary()
    {
        if (!fs::exists(project_summary["project_output"].Scalar()))
            fs::create_directories(project_summary["project_output"].Scalar());

        std::ofstream summary_file( project_summary["project_output"].Scalar() + "/bob_summary.yaml" );
        summary_file << project_summary;
        summary_file.close();
        std::ofstream json_file( project_summary["project_output"].Scalar() + "/bob_summary.json" );
		json_file << project_summary_json.dump(3);
		json_file.close();
    }

    void project::load_component_registries()
    {
        // Verify the .bob/registries path exists
    	if (!fs::exists(this->project_directory + "/.bob/registries"))
            return;

        for ( const auto& p : fs::recursive_directory_iterator( this->project_directory + "/.bob/registries") )
            if ( p.path().extension().generic_string() == ".yaml" )
                try
                {
                    registries[p.path().filename().replace_extension().generic_string()] = YAML::LoadFile(p.path().generic_string());
                }
                catch (...)
                {
                    log->error("Could not parse component registry: '{}'", p.path().generic_string());
                }
    }

    std::optional<YAML::Node> project::find_registry_component(const std::string& name)
    {
        // Look for component in registries
        for ( auto r : registries )
            if ( r.second["provides"]["components"][name].IsDefined( ) )
                return r.second["provides"]["components"][name];
        return {};
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
        do
        {
            std::getline(infile, line);
        } while(line.length() > 0 && line.find(':') == std::string::npos);

        while (std::getline(infile, line, ' '))
        {
            if (line.empty() || line.compare("\\\n") == 0)
                continue;
            if (line.back() == '\n') line.pop_back();
            if (line.back() == '\r') line.pop_back();
            dependencies.push_back(line);
        }

        return dependencies;
    }

    /**
     * @brief Returns the path corresponding to the home directory of BOB
     *        Typically this would be ~/.bob or /Users/<username>/.bob or $HOME/.bob
     * @return std::string
     */
    std::string get_bob_home()
    {
        std::string home = !std::getenv("HOME") ? std::getenv("HOME") : std::getenv("USERPROFILE");
        return home + "/.bob";
    }

} /* namespace bob */
