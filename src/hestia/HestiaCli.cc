#include "HestiaCli.h"

#include "HestiaClient.h"

#include "FileStreamSink.h"
#include "FileStreamSource.h"

#include "ConsoleInterface.h"
#include "DaemonManager.h"
#include "ErrorUtils.h"
#include "Logger.h"
#include "UuidUtils.h"

#include "ProjectConfig.h"

#include <CLI/CLI11.hpp>
#include <iostream>

namespace hestia {

HestiaCli::HestiaCli(std::unique_ptr<IConsoleInterface> console_interface) :
    m_console_interface(
        console_interface == nullptr ? std::make_unique<ConsoleInterface>() :
                                       std::move(console_interface))
{
}

void HestiaCli::add_hsm_actions(
    std::unordered_map<std::string, CLI::App*>& commands,
    CLI::App* command,
    const std::string& subject)
{
    HsmAction::Action_enum_string_converter converter;
    converter.init();

    for (const auto& action : m_client_command.get_subject_actions(subject)) {
        const auto action_name = converter.to_string(action);
        const auto tag         = subject + "_" + action_name;

        switch (action) {
            case HsmAction::Action::GET_DATA:
                commands[tag] = command->add_subcommand(
                    action_name, "Get " + subject + " data");
                add_get_data_options(commands[tag]);
                break;
            case HsmAction::Action::PUT_DATA:
                commands[tag] = command->add_subcommand(
                    action_name, "Put " + subject + " data");
                add_put_data_options(commands[tag]);
                break;
            case HsmAction::Action::COPY_DATA:
                commands[tag] = command->add_subcommand(
                    action_name, "Copy a " + subject + " between tiers");
                add_copy_data_options(commands[tag]);
                break;
            case HsmAction::Action::MOVE_DATA:
                commands[tag] = command->add_subcommand(
                    action_name, "Move a " + subject + " between tiers");
                add_move_data_options(commands[tag]);
                break;
            case HsmAction::Action::RELEASE_DATA:
                commands[tag] = command->add_subcommand(
                    action_name, "Release a " + subject + " from a tier");
                add_move_data_options(commands[tag]);
                break;
            case HsmAction::Action::CRUD:
            case HsmAction::Action::NONE:
            default:
                continue;
        };
        commands[tag]
            ->add_option("id", m_client_command.m_id, "Id")
            ->required();
    }
}

void HestiaCli::add_get_data_options(CLI::App* command)
{
    command
        ->add_option("--file", m_client_command.m_path, "Path to write data to")
        ->required();
    command->add_option(
        "--tier", m_client_command.m_source_tier, "Tier to get data from");
}

void HestiaCli::add_put_data_options(CLI::App* command)
{
    command
        ->add_option(
            "--file", m_client_command.m_path, "Path to read data from")
        ->required();
    command->add_option(
        "--tier", m_client_command.m_target_tier, "Tier to put data to");
}

void HestiaCli::add_copy_data_options(CLI::App* command)
{
    command
        ->add_option("--source", m_client_command.m_source_tier, "Source Tier")
        ->required();
    command
        ->add_option("--target", m_client_command.m_target_tier, "Target Tier")
        ->required();
}

void HestiaCli::add_move_data_options(CLI::App* command)
{
    command
        ->add_option("--source", m_client_command.m_source_tier, "Source Tier")
        ->required();
    command
        ->add_option("--target", m_client_command.m_target_tier, "Target Tier")
        ->required();
}

void HestiaCli::add_release_data_options(CLI::App* command)
{
    command
        ->add_option(
            "--tier", m_client_command.m_source_tier, "Tier to remove from")
        ->required();
}

void HestiaCli::add_crud_commands(
    std::unordered_map<std::string, CLI::App*>& commands,
    CLI::App& app,
    const std::string& subject)
{
    commands[subject] = app.add_subcommand(subject, subject + " commands");

    auto create_cmd =
        commands[subject]->add_subcommand("create", "Create a " + subject);
    create_cmd->add_option(
        "--id_fmt", m_client_command.m_id_format, "Id Format Specifier");
    create_cmd->add_option(
        "--input_fmt", m_client_command.m_input_format,
        "Input Format Specifier");
    create_cmd->add_option(
        "--output_fmt", m_client_command.m_output_format,
        "Output Format Specifier");
    create_cmd->add_option("id", m_client_command.m_id, "Subject Id");
    commands[subject + "_create"] = create_cmd;

    auto update_cmd =
        commands[subject]->add_subcommand("update", "Update a " + subject);
    update_cmd->add_option(
        "--id_fmt", m_client_command.m_id_format, "Id Format Specifier");
    update_cmd->add_option(
        "--input_fmt", m_client_command.m_input_format,
        "Input Format Specifier");
    update_cmd->add_option(
        "--output_fmt", m_client_command.m_output_format,
        "Output Format Specifier");
    update_cmd->add_option("id", m_client_command.m_id, "Subject Id");
    commands[subject + "_update"] = update_cmd;

    auto read_cmd =
        commands[subject]->add_subcommand("read", "Read " + subject + "s");
    read_cmd->add_option(
        "--query_fmt", m_client_command.m_input_format,
        "Query Format Specifier");
    read_cmd->add_option(
        "--output_fmt", m_client_command.m_output_format,
        "Output Format Specifier");
    read_cmd->add_option(
        "--offset", m_client_command.m_offset, "Page start offset");
    read_cmd->add_option(
        "--count", m_client_command.m_count, "Max number of items per page");
    read_cmd->add_option("query", m_client_command.m_body, "Query body");
    commands[subject + "_read"] = read_cmd;

    auto remove_cmd =
        commands[subject]->add_subcommand("remove", "Remove a " + subject);
    remove_cmd->add_option(
        "--id_fmt", m_client_command.m_id_format, "Id Format Specifier");
    remove_cmd->add_option("id", m_client_command.m_id, "Subject Id");
    commands[subject + "_remove"] = remove_cmd;

    auto identify_cmd =
        commands[subject]->add_subcommand("identify", "Identify a " + subject);
    identify_cmd->add_option(
        "--id_fmt", m_client_command.m_id_format, "Id Format Spec");
    identify_cmd->add_option(
        "--output_fmt", m_client_command.m_output_format,
        "Output Format Specifier");
    commands[subject + "_identify"] = identify_cmd;
}

void HestiaCli::parse_args(int argc, char* argv[])
{
    CLI::App app{
        "Hestia - Hierarchical Storage Tiers Interface for Applications",
        "hestia"};

    std::unordered_map<std::string, CLI::App*> commands;

    for (const auto& subject : m_client_command.get_hsm_subjects()) {
        add_crud_commands(commands, app, subject);
    }

    for (const auto& subject : m_client_command.get_system_subjects()) {
        add_crud_commands(commands, app, subject);
    }

    for (const auto& subject : m_client_command.get_action_subjects()) {
        add_hsm_actions(commands, commands[subject], subject);
    }

    commands["server"] = app.add_subcommand("server", "Run the Hestia Server");

    commands["start"] = app.add_subcommand("start", "Start the Hestia Daemon");
    commands["stop"]  = app.add_subcommand("stop", "Stop the Hestia Daemon");

    app.add_flag(
        "--version", m_client_command.m_is_version,
        "Print application version");

    for (const auto& [key, command] : commands) {
        if (!command->get_subcommands().empty()) {
            continue;
        }

        command->add_flag(
            "--verbose", m_client_command.m_is_verbose,
            "Print extra diagnostics to Stderr");
        command->add_option(
            "-c, --config", m_config_path, "Path to a Hestia config file.");
        command->add_option(
            "-t, --token", m_user_token, "User authentication token.");
        command->add_option(
            "--host", m_server_host, "Hestia server host address");
        command->add_option(
            "-p, --port", m_server_port, "Hestia server host port");
    }

    try {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError& e) {
        app.exit(e);
        throw std::runtime_error(
            "Returning after command line parsing. --help or invalid argument(s)");
    }

    bool parsed{false};
    HsmAction::Action_enum_string_converter action_string_converter;
    action_string_converter.init();

    for (const auto& subject : m_client_command.get_hsm_subjects()) {
        for (const auto& method : m_client_command.get_crud_methods()) {
            auto command = commands[subject + "_" + method];
            if (command != nullptr && command->parsed()) {
                m_client_command.set_crud_method(method);
                m_client_command.set_hsm_subject(subject);
                parsed = true;
                break;
            }
        }
        for (const auto& action :
             m_client_command.get_subject_actions(subject)) {
            const auto action_str = action_string_converter.to_string(action);
            if (commands[subject + "_" + action_str]->parsed()) {
                m_client_command.set_hsm_action(action);
                m_client_command.set_hsm_subject(subject);
                parsed = true;
                break;
            }
        }
    }

    for (const auto& subject : m_client_command.get_system_subjects()) {
        for (const auto& method : m_client_command.get_crud_methods()) {
            auto command = commands[subject + "_" + method];
            if (command != nullptr && command->parsed()) {
                m_client_command.set_crud_method(method);
                m_client_command.set_system_subject(subject);
                parsed = true;
                break;
            }
        }
    }

    if (parsed) {
        m_app_command = AppCommand::CLIENT;
        return;
    }

    if (m_client_command.m_is_version) {
        print_version();
        return;
    }
    else if (commands["start"]->parsed()) {
        m_app_command = AppCommand::DAEMON_START;
    }
    else if (commands["stop"]->parsed()) {
        m_app_command = AppCommand::DAEMON_STOP;
    }
    else if (commands["server"]->parsed()) {
        m_app_command = AppCommand::SERVER;
    }

    if (m_app_command == AppCommand::UNKNOWN) {
        std::cerr << "Hestia: Empty CLI arguments. Use --help for usage: \n"
                  << app.help() << std::endl;
        throw std::runtime_error(
            "Returning after command line parsing. --help or invalid argument(s)");
    }
}

bool HestiaCli::is_client() const
{
    return m_app_command == AppCommand::CLIENT;
}

bool HestiaCli::is_server() const
{
    return m_app_command == AppCommand::DAEMON_START
           || m_app_command == AppCommand::SERVER;
}

bool HestiaCli::is_daemon() const
{
    return m_app_command == AppCommand::DAEMON_START
           || m_app_command == AppCommand::DAEMON_STOP;
}

OpStatus HestiaCli::run(IHestiaApplication* app)
{
    if (m_client_command.m_is_version) {
        return {};
    }

    if (app == nullptr || m_app_command == AppCommand::DAEMON_STOP) {
        return stop_daemon();
    }
    else if (m_app_command == AppCommand::DAEMON_START) {
        return start_daemon(app);
    }
    else if (m_app_command == AppCommand::SERVER) {
        return run_server(app);
    }
    else if (m_app_command == AppCommand::CLIENT) {
        return run_client(app);
    }
    else {
        return {
            OpStatus::Status::ERROR, -1,
            "CLI command not supported. Use --help for options."};
    }
}

OpStatus HestiaCli::print_info(IHestiaApplication* app)
{
    m_console_interface->console_write_error(app->get_runtime_info());
    return {};
}

void HestiaCli::print_version()
{
    m_console_interface->console_write(
        hestia::project_config::get_project_name()
        + " version: " + hestia::project_config::get_project_version());
}

OpStatus HestiaCli::run_client(IHestiaApplication* app)
{
    OpStatus status;
    try {
        status = app->initialize(
            m_config_path, m_user_token, {}, m_server_host, m_server_port);
    }
    catch (const std::exception& e) {
        status = OpStatus(OpStatus::Status::ERROR, -1, e.what());
    }
    catch (...) {
        status = OpStatus(
            OpStatus::Status::ERROR, -1,
            "Unknown exception initializing client.");
    }

    if (!status.ok()) {
        return status;
    }

    if (m_client_command.m_is_verbose) {
        print_info(app);
    }

    auto client = dynamic_cast<IHestiaClient*>(app);
    if (client == nullptr) {
        return {
            OpStatus::Status::ERROR, -1,
            "Invalid app type passed to run_client."};
    }

    if (m_client_command.is_crud_method()) {
        return on_crud_method(client);
    }
    else if (m_client_command.is_data_management_action()) {
        m_client_command.m_action.set_subject(
            m_client_command.m_subject.m_hsm_type);
        m_client_command.m_action.set_target_tier(
            m_client_command.m_target_tier);
        m_client_command.m_action.set_source_tier(
            m_client_command.m_source_tier);
        if (!m_client_command.m_id.empty()) {
            m_client_command.m_action.set_subject_key(m_client_command.m_id[0]);
        }

        const auto status =
            client->do_data_movement_action(m_client_command.m_action);
        if (status.ok()) {
            m_console_interface->console_write(m_client_command.m_action.id());
        }
        return status;
    }
    else if (m_client_command.is_data_io_action()) {
        Stream stream;
        OpStatus status;
        m_client_command.m_action.set_subject(
            m_client_command.m_subject.m_hsm_type);
        m_client_command.m_action.set_target_tier(
            m_client_command.m_target_tier);
        m_client_command.m_action.set_source_tier(
            m_client_command.m_source_tier);
        if (!m_client_command.m_id.empty()) {
            m_client_command.m_action.set_subject_key(m_client_command.m_id[0]);
        }
        if (m_client_command.is_data_put_action()) {
            stream.set_source(
                FileStreamSource::create(m_client_command.m_path));
            m_client_command.m_action.set_size(stream.get_source_size());
        }
        else {
            stream.set_sink(FileStreamSink::create(m_client_command.m_path));
        }

        auto completion_cb =
            [&status, this](OpStatus ret_status, const HsmAction& action) {
                status = ret_status;
                m_console_interface->console_write(action.get_primary_key());
            };

        client->do_data_io_action(
            m_client_command.m_action, &stream, completion_cb);

        if (m_client_command.is_data_put_action()) {
            if (stream.waiting_for_content()) {
                auto result = stream.flush();
                if (!result.ok()) {
                    const auto msg =
                        "Failed to flush stream with: " + result.to_string();
                    LOG_ERROR(msg);
                    return {OpStatus::Status::ERROR, -1, msg};
                }
            }
        }
        else if (stream.has_content()) {
            auto result = stream.flush();
            if (!result.ok()) {
                const auto msg =
                    "Failed to flush stream with: " + result.to_string();
                LOG_ERROR(msg);
                return {OpStatus::Status::ERROR, -1, msg};
            }
        }
        return status;
    }
    return {
        OpStatus::Status::ERROR, -1, "Unsupported client method requested."};
}

OpStatus HestiaCli::on_crud_method(IHestiaClient* client)
{
    if (m_client_command.is_create_method()
        || m_client_command.is_update_method()) {
        const bool is_create = m_client_command.is_create_method();

        VecCrudIdentifier ids;
        CrudAttributes attributes;
        const auto& [output_format, output_attr_format] =
            m_client_command.parse_create_update_inputs(
                ids, attributes, m_console_interface.get());
        if (is_create) {
            if (const auto status = client->create(
                    m_client_command.m_subject, ids, attributes,
                    output_attr_format);
                !status.ok()) {
                return status;
            }
        }
        else {
            if (const auto status = client->update(
                    m_client_command.m_subject, ids, attributes,
                    output_attr_format);
                !status.ok()) {
                return status;
            }
        }
        const bool should_write_attributes =
            HestiaClientCommand::expects_attributes(output_format)
            && !attributes.buffer().empty();
        if (HestiaClientCommand::expects_id(output_format)) {
            for (const auto& id : ids) {
                m_console_interface->console_write(id.get_primary_key());
            }
            if (should_write_attributes) {
                m_console_interface->console_write("");
            }
        }
        if (should_write_attributes) {
            m_console_interface->console_write(attributes.buffer());
        }
    }
    else if (m_client_command.is_read_method()) {
        LOG_INFO(
            "CLI Read request for type: "
            << HestiaType::to_string(m_client_command.m_subject));
        CrudQuery query;
        const auto& [output_format, output_attr_format] =
            m_client_command.parse_read_inputs(
                query, m_console_interface.get());
        if (const auto status = client->read(m_client_command.m_subject, query);
            !status.ok()) {
            return status;
        }
        const bool should_write_attributes =
            HestiaClientCommand::expects_attributes(output_format)
            && !query.get_attributes().get_buffer().empty();

        if (HestiaClientCommand::expects_id(output_format)) {
            for (const auto& id : query.get_ids()) {
                m_console_interface->console_write(id.get_primary_key());
            }
            if (should_write_attributes) {
                m_console_interface->console_write("");
            }
        }
        if (should_write_attributes) {
            m_console_interface->console_write(
                query.get_attributes().get_buffer());
        }
    }
    else if (m_client_command.is_remove_method()) {
        LOG_INFO("CLI Remove request");
        VecCrudIdentifier ids;
        m_client_command.parse_remove_inputs(ids, m_console_interface.get());
        return client->remove(m_client_command.m_subject, ids);
    }
    return {};
}

OpStatus HestiaCli::run_server(IHestiaApplication* app)
{
    OpStatus status;
    try {
        status = app->initialize(m_config_path, m_user_token);
    }
    catch (const std::exception& e) {
        status = OpStatus(OpStatus::Status::ERROR, -1, e.what());
    }
    catch (...) {
        status = OpStatus(
            OpStatus::Status::ERROR, -1,
            "Unknown exception initializing server.");
    }

    if (!status.ok()) {
        return status;
    }

    try {
        status = app->run();
    }
    catch (const std::exception& e) {
        status = OpStatus(OpStatus::Status::ERROR, -1, e.what());
    }
    catch (...) {
        status = OpStatus(
            OpStatus::Status::ERROR, -1, "Unknown exception running server.");
    }
    return status;
}

OpStatus HestiaCli::start_daemon(IHestiaApplication* app)
{
    DaemonManager daemon_manager;
    auto rc = daemon_manager.start();
    if (rc == DaemonManager::Status::EXIT_OK) {
        return {};
    }
    else if (rc == DaemonManager::Status::EXIT_FAILED) {
        return OpStatus(
            OpStatus::Status::ERROR, 0, "Error starting hestia Daemon");
    }
    return run_server(app);
}

OpStatus HestiaCli::stop_daemon()
{
    DaemonManager daemon_manager;
    const auto rc = daemon_manager.stop();
    if (rc == DaemonManager::Status::EXIT_OK) {
        return {};
    }
    else if (rc == DaemonManager::Status::EXIT_FAILED) {
        return OpStatus(
            OpStatus::Status::ERROR, 0, "Error stopping hestia Daemon");
    }
    return {};
}

}  // namespace hestia