// ---------------------------------------------------------------------
//
// Copyright (c) 2014 - 2026 by the IBAMR developers
// All rights reserved.
//
// This file is part of IBAMR.
//
// IBAMR is free software and is distributed under the 3-clause BSD
// license. The full text of the license can be found in the file
// COPYRIGHT at the top level directory of IBAMR.
//
// ---------------------------------------------------------------------

/////////////////////////////// INCLUDES /////////////////////////////////////

#include <ibtk/AppInitializer.h>
#include <ibtk/IBTK_CHKERRQ.h>
#include <ibtk/IBTK_MPI.h>
#include <ibtk/LSiloDataWriter.h>

#include <tbox/Array.h>
#include <tbox/Database.h>
#include <tbox/InputDatabase.h>
#include <tbox/InputManager.h>
#include <tbox/NullDatabase.h>
#include <tbox/PIO.h>
#include <tbox/Pointer.h>
#include <tbox/RestartManager.h>
#include <tbox/TimerManager.h>
#include <tbox/Utilities.h>

#include <petsclog.h>
#include <petscoptions.h>
#include <petscsys.h>

#include <VisItDataWriter.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <ibtk/namespaces.h> // IWYU pragma: keep

/////////////////////////////// NAMESPACE ////////////////////////////////////

namespace IBTK
{
/////////////////////////////// STATIC ///////////////////////////////////////

namespace
{
struct PetscOption
{
    std::string name;
    std::string value;
};

std::filesystem::path
resolve_petsc_options_file(const std::filesystem::path& advertised_path, const std::filesystem::path& input_filename)
{
    std::string resolved_path_string;
    std::string error_message;
    if (IBTK_MPI::getRank() == 0)
    {
        std::error_code error_code;
        bool path_exists = std::filesystem::exists(advertised_path, error_code);
        if (error_code)
        {
            error_message = "IBTK_PETSC_OPTIONS_FILESYSTEM_ERROR: could not inspect PETSc options file '" +
                            advertised_path.string() + "': " + error_code.message();
        }

        std::filesystem::path resolved_path = advertised_path;
        if (error_message.empty() && !path_exists)
        {
            const std::filesystem::path canonical_input = std::filesystem::weakly_canonical(input_filename, error_code);
            if (error_code)
            {
                error_message = "IBTK_PETSC_OPTIONS_FILESYSTEM_ERROR: could not resolve input file '" +
                                input_filename.string() + "': " + error_code.message();
            }
            else
            {
                resolved_path = canonical_input.parent_path() / advertised_path;
                path_exists = std::filesystem::exists(resolved_path, error_code);
                if (error_code)
                {
                    error_message = "IBTK_PETSC_OPTIONS_FILESYSTEM_ERROR: could not inspect PETSc options file '" +
                                    resolved_path.string() + "': " + error_code.message();
                }
            }
        }

        if (error_message.empty() && !path_exists)
        {
            error_message =
                "IBTK_PETSC_OPTIONS_FILE_MISSING: could not open PETSc options file '" + advertised_path.string() + "'";
        }
        if (error_message.empty())
        {
            const std::filesystem::path canonical_path = std::filesystem::weakly_canonical(resolved_path, error_code);
            if (error_code)
            {
                error_message = "IBTK_PETSC_OPTIONS_FILESYSTEM_ERROR: could not resolve PETSc options file '" +
                                resolved_path.string() + "': " + error_code.message();
            }
            else
            {
                resolved_path_string = canonical_path.string();
            }
        }
    }

    IBTK_MPI::bcast(error_message, 0);
    if (!error_message.empty()) TBOX_ERROR(error_message << '\n');
    IBTK_MPI::bcast(resolved_path_string, 0);
    return std::filesystem::path(resolved_path_string);
}

std::string
lowercase(std::string value)
{
    const auto& facet = std::use_facet<std::ctype<char>>(std::locale::classic());
    facet.tolower(value.data(), value.data() + value.size());
    return value;
}

bool
is_settings_key(const std::string& key)
{
    return key == "petsc_settings" || key.compare(0, 15, "petsc_settings_") == 0;
}

std::vector<std::string>
sorted_keys(Pointer<Database> db)
{
    const Array<std::string> keys = db->getAllKeys();
    std::vector<std::string> result;
    for (int k = 0; k < keys.size(); ++k) result.push_back(keys[k]);
    std::sort(result.begin(), result.end());
    return result;
}

struct SettingsBlock
{
    Pointer<Database> db;
    std::string path;
    std::string prefix;
};

void
collect_settings(Pointer<Database> db, const std::string& path, std::vector<SettingsBlock>& blocks)
{
    for (const std::string& key : sorted_keys(db))
    {
        const std::string entry_path = path + "::" + key;
        if (is_settings_key(key))
        {
            std::string prefix;
            if (key != "petsc_settings")
            {
                const std::string tag = key.substr(15);
                const auto& locale = std::locale::classic();
                const bool valid = !tag.empty() && std::isalpha(tag.front(), locale) &&
                                   std::isalnum(tag.back(), locale) &&
                                   std::all_of(tag.begin(),
                                               tag.end(),
                                               [&locale](const char c) { return std::isalnum(c, locale) || c == '_'; });
                if (!valid) throw std::invalid_argument("IBTK_PETSC_SETTINGS_TAG: invalid tag at " + entry_path);
                prefix = tag + "_";
            }
            if (!db->isDatabase(key))
                throw std::invalid_argument("IBTK_PETSC_SETTINGS_BLOCK: expected database at " + entry_path);
            blocks.push_back({ db->getDatabase(key), entry_path, prefix });
        }
        else if (db->isDatabase(key))
        {
            collect_settings(db->getDatabase(key), entry_path, blocks);
        }
    }
}

template <class T>
std::string
floating_point_to_string(const T value, const std::string& path)
{
    if (!std::isfinite(value)) throw std::invalid_argument("IBTK_PETSC_SETTINGS_NONFINITE: nonfinite value at " + path);
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<T>::max_digits10) << value;
    return stream.str();
}

std::vector<PetscOption>
validate_settings(const std::vector<SettingsBlock>& blocks)
{
    const std::set<std::string> controls = { "options_file", "options_file_yaml", "options_file_yaml_directory",
                                             "prefix_pop",   "prefix_push",       "skip_petscrc" };
    std::map<std::string, std::string> definitions;
    std::vector<PetscOption> options;
    for (const SettingsBlock& block : blocks)
    {
        for (const std::string& key : sorted_keys(block.db))
        {
            const std::string path = block.path + "::" + key;
            if (key.empty() || key.front() == '-')
                throw std::invalid_argument("IBTK_PETSC_SETTINGS_NAME: invalid leaf key at " + path);
            const std::string name = "-" + block.prefix + key;
            if (name.size() > 255)
                throw std::invalid_argument("IBTK_PETSC_SETTINGS_LENGTH: expanded name exceeds 255 bytes at " + path);
            PetscBool valid_key = PETSC_FALSE;
            int ierr = PetscOptionsValidKey(name.c_str(), &valid_key);
            IBTK_CHKERRQ(ierr);
            if (!valid_key)
                throw std::invalid_argument("IBTK_PETSC_SETTINGS_NAME: invalid expanded PETSc name at " + path);
            if (controls.count(lowercase(name.substr(1))))
                throw std::invalid_argument("IBTK_PETSC_SETTINGS_CONTROL: unsupported parser control at " + path);

            const auto inserted = definitions.emplace(lowercase(name), path);
            if (!inserted.second)
                throw std::invalid_argument("IBTK_PETSC_SETTINGS_DUPLICATE: " + inserted.first->second + " and " +
                                            path + " both define " + name);
            if (block.db->isDatabase(key))
                throw std::invalid_argument("IBTK_PETSC_SETTINGS_NESTED: settings must be flat at " + path);
            if (block.db->getArraySize(key) != 1)
                throw std::invalid_argument(
                    "IBTK_PETSC_SETTINGS_ARRAY: expected a scalar Boolean, integer, "
                    "floating-point value, or string at " +
                    path);

            std::string value;
            switch (block.db->getArrayType(key))
            {
            case Database::SAMRAI_BOOL:
                value = block.db->getBool(key) ? "true" : "false";
                break;
            case Database::SAMRAI_INT:
                value = std::to_string(block.db->getInteger(key));
                break;
            case Database::SAMRAI_FLOAT:
                value = floating_point_to_string(block.db->getFloat(key), path);
                break;
            case Database::SAMRAI_DOUBLE:
                value = floating_point_to_string(block.db->getDouble(key), path);
                break;
            case Database::SAMRAI_STRING:
                value = block.db->getString(key);
                if (value.empty())
                    throw std::invalid_argument("IBTK_PETSC_SETTINGS_EMPTY_STRING: empty string at " + path);
                break;
            default:
                throw std::invalid_argument(
                    "IBTK_PETSC_SETTINGS_TYPE: expected a scalar Boolean, integer, "
                    "floating-point value, or string at " +
                    path);
            }
            options.push_back({ name, value });
        }
    }
    std::sort(
        options.begin(), options.end(), [](const PetscOption& a, const PetscOption& b) { return a.name < b.name; });
    return options;
}

void
insert_petsc_options_file(const std::filesystem::path& advertised_path,
                          const std::filesystem::path& input_filename,
                          int argc,
                          char* argv[])
{
    const std::filesystem::path resolved_path = resolve_petsc_options_file(advertised_path, input_filename);
    const std::string resolved_path_string = resolved_path.string();

    std::vector<std::string> argument_storage;
    argument_storage.reserve(argc);
    for (int k = 0; k < argc; ++k) argument_storage.emplace_back(argv[k]);
    std::vector<char*> arguments;
    arguments.reserve(argc + 1);
    for (std::string& argument : argument_storage) arguments.push_back(argument.data());
    arguments.push_back(nullptr);
    int local_argc = argc;
    char** local_argv = arguments.data();

    int ierr = PetscOptionsInsert(nullptr, &local_argc, &local_argv, resolved_path_string.c_str());
    IBTK_CHKERRQ(ierr);
}

} // namespace

void
AppInitializer::insertPetscSettings(Pointer<Database> input_db)
{
    std::vector<SettingsBlock> blocks;
    collect_settings(input_db, "input", blocks);
    if (!blocks.empty() && (input_db->keyExists("PETSC_OPTIONS_FILE") || input_db->keyExists("petsc_options_file")))
        throw std::invalid_argument(
            "IBTK_PETSC_SETTINGS_SOURCE_CONFLICT: settings and root PETSc options-file keys "
            "are mutually exclusive");

    // Validate the entire collection before HasName() changes used-state or SetValue() inserts anything.
    const std::vector<PetscOption> options = validate_settings(blocks);
    for (const PetscOption& option : options)
    {
        PetscBool present = PETSC_FALSE;
        int ierr = PetscOptionsHasName(nullptr, nullptr, option.name.c_str(), &present);
        IBTK_CHKERRQ(ierr);
        if (!present)
        {
            ierr = PetscOptionsSetValue(nullptr, option.name.c_str(), option.value.c_str());
            IBTK_CHKERRQ(ierr);
        }
    }
}

/////////////////////////////// PUBLIC ///////////////////////////////////////

AppInitializer::AppInitializer(int argc, char* argv[], const std::string& default_log_file_name)
{
    if (argc == 1)
    {
        TBOX_ERROR("USAGE: " << argv[0] << " <input filename> <restart dir> <restore number> [options]\n"
                             << "OPTIONS: PETSc command line options; use -help for more information.\n");
    }

    // Process command line options.
    const std::string input_filename = argv[1];
    if (argc >= 4)
    {
        // Check whether this appears to be a restarted run.
        FILE* fstream = (IBTK_MPI::getRank() == 0 ? fopen(argv[2], "r") : nullptr);
        if (IBTK_MPI::bcast(fstream ? 1 : 0, 0) == 1)
        {
            d_restart_read_dirname = argv[2];
            d_restart_restore_num = atoi(argv[3]);
            d_is_from_restart = true;
        }
        if (fstream)
        {
            fclose(fstream);
        }
    }

    // Process restart data if this is a restarted run.
    if (d_is_from_restart)
    {
        RestartManager::getManager()->openRestartFile(
            d_restart_read_dirname, d_restart_restore_num, IBTK_MPI::getNodes());
    }

    // Create input database and parse all data in input file.
    d_input_db = new InputDatabase("input_db");
    InputManager::getManager()->parseInputFile(input_filename, d_input_db);

    // Process "Main" section of the input database.
    Pointer<Database> main_db = new NullDatabase();
    if (d_input_db->isDatabase("Main"))
    {
        main_db = d_input_db->getDatabase("Main");
    }

    // Configure ordinary PETSc options used after application initialization.
    try
    {
        insertPetscSettings(d_input_db);
    }
    catch (const std::invalid_argument& error)
    {
        TBOX_ERROR(error.what() << '\n');
    }
    if (d_input_db->keyExists("PETSC_OPTIONS_FILE"))
        insert_petsc_options_file(d_input_db->getString("PETSC_OPTIONS_FILE"), input_filename, argc, argv);
    else if (d_input_db->keyExists("petsc_options_file"))
        insert_petsc_options_file(d_input_db->getString("petsc_options_file"), input_filename, argc, argv);

    // Configure logging options.
    std::string log_file_name = default_log_file_name;
    bool log_all_nodes = false;
    if (main_db->keyExists("log_file_name")) log_file_name = main_db->getString("log_file_name");
    if (main_db->keyExists("log_all_nodes")) log_all_nodes = main_db->getBool("log_all_nodes");
    if (!log_file_name.empty())
    {
        if (log_all_nodes)
        {
            PIO::logAllNodes(log_file_name);
        }
        else
        {
            PIO::logOnlyNodeZero(log_file_name);
        }
    }

    // Configure timer options.
    std::string timer_dump_interval_key_name;
    if (main_db->keyExists("timer_interval"))
    {
        timer_dump_interval_key_name = "timer_interval";
    }
    else if (main_db->keyExists("timer_dump_interval"))
    {
        timer_dump_interval_key_name = "timer_dump_interval";
    }
    else if (main_db->keyExists("timer_write_interval"))
    {
        timer_dump_interval_key_name = "timer_write_interval";
    }

    if (!timer_dump_interval_key_name.empty())
    {
        d_timer_dump_interval = main_db->getInteger(timer_dump_interval_key_name);
    }

    // Avoid some warnings by unconditionally creating the timer database, even if
    // we never use it:
    {
        Pointer<Database> timer_manager_db;
        if (d_input_db->isDatabase("TimerManager"))
        {
            timer_manager_db = d_input_db->getDatabase("TimerManager");
        }
        TimerManager::createManager(timer_manager_db);
    }

    // Configure visualization options.
    std::string viz_dump_interval_key_name;
    if (main_db->keyExists("viz_interval"))
    {
        viz_dump_interval_key_name = "viz_interval";
    }
    else if (main_db->keyExists("viz_dump_interval"))
    {
        viz_dump_interval_key_name = "viz_dump_interval";
    }
    else if (main_db->keyExists("viz_write_interval"))
    {
        viz_dump_interval_key_name = "viz_write_interval";
    }

    std::string viz_dump_dirname_key_name;
    if (main_db->keyExists("viz_dirname"))
    {
        viz_dump_dirname_key_name = "viz_dirname";
    }
    else if (main_db->keyExists("viz_dump_dirname"))
    {
        viz_dump_dirname_key_name = "viz_dump_dirname";
    }
    else if (main_db->keyExists("viz_write_dirname"))
    {
        viz_dump_dirname_key_name = "viz_write_dirname";
    }

    if (!viz_dump_interval_key_name.empty())
    {
        d_viz_dump_interval = main_db->getInteger(viz_dump_interval_key_name);
        if (!viz_dump_dirname_key_name.empty())
        {
            d_viz_dump_dirname = main_db->getString(viz_dump_dirname_key_name);
            if (d_viz_dump_dirname.empty())
            {
                pout << "WARNING: AppInitializer::AppInitializer(): " << viz_dump_interval_key_name << " > 0, but `"
                     << d_viz_dump_dirname << "' is empty\n";
            }
        }
        else
        {
            pout << "WARNING: AppInitializer::AppInitializer(): " << viz_dump_interval_key_name
                 << " > 0, but `viz_dump_dirname' is not specified in input file\n";
        }
    }

    std::string viz_writers_key_name;
    Array<std::string> viz_writers_arr;
    if (main_db->keyExists("viz_writer"))
    {
        viz_writers_key_name = "viz_writer";
    }
    else if (main_db->keyExists("viz_writers"))
    {
        viz_writers_key_name = "viz_writers";
    }

    if (!viz_writers_key_name.empty())
    {
        viz_writers_arr = main_db->getStringArray(viz_writers_key_name);
    }
    if (viz_writers_arr.size() > 0)
    {
        d_viz_writers = std::vector<std::string>(viz_writers_arr.getPointer(),
                                                 viz_writers_arr.getPointer() + viz_writers_arr.size());
    }

    if (d_viz_dump_interval == 0 && d_viz_writers.size() > 0)
    {
        if (main_db->keyExists(viz_dump_dirname_key_name))
        {
            d_viz_dump_dirname = main_db->getString(viz_dump_dirname_key_name);
            if (d_viz_dump_dirname.empty())
            {
                pout << "WARNING: AppInitializer::AppInitializer(): `" << viz_writers_key_name << "' is set, but `"
                     << viz_dump_dirname_key_name << "' is empty\n";
            }
        }
        else
        {
            pout << "WARNING: AppInitializer::AppInitializer(): `" << viz_writers_key_name
                 << "' is set, but key `viz_dump_dirname' not specifed in input file\n";
        }
    }

    for (const auto& viz_writer : d_viz_writers)
    {
        if (viz_writer == "VisIt")
        {
            int visit_number_procs_per_file = 1;
            if (main_db->keyExists("visit_number_procs_per_file"))
                visit_number_procs_per_file = main_db->getInteger("visit_number_procs_per_file");
            d_visit_data_writer =
                new VisItDataWriter<NDIM>("VisItDataWriter", d_viz_dump_dirname, visit_number_procs_per_file);
        }

        if (viz_writer == "Silo")
        {
            d_silo_data_writer = new LSiloDataWriter("LSiloDataWriter", d_viz_dump_dirname);
        }

        if (viz_writer == "ExodusII")
        {
            if (main_db->keyExists("exodus_filename")) d_exodus_filename = main_db->getString("exodus_filename");
        }

        if (viz_writer == "GMV")
        {
            if (main_db->keyExists("gmv_filename")) d_gmv_filename = main_db->getString("gmv_filename");
        }
    }

    // Configure restart options.
    std::string restart_dump_interval_key_name;
    if (main_db->keyExists("restart_interval"))
    {
        restart_dump_interval_key_name = "restart_interval";
    }
    else if (main_db->keyExists("restart_dump_interval"))
    {
        restart_dump_interval_key_name = "restart_dump_interval";
    }
    else if (main_db->keyExists("restart_write_interval"))
    {
        restart_dump_interval_key_name = "restart_write_interval";
    }

    std::string restart_dump_dirname_key_name;
    if (main_db->keyExists("restart_dirname"))
    {
        restart_dump_dirname_key_name = "restart_dirname";
    }
    else if (main_db->keyExists("restart_dump_dirname"))
    {
        restart_dump_dirname_key_name = "restart_dump_dirname";
    }
    else if (main_db->keyExists("restart_write_dirname"))
    {
        restart_dump_dirname_key_name = "restart_write_dirname";
    }

    if (!restart_dump_interval_key_name.empty())
    {
        d_restart_dump_interval = main_db->getInteger(restart_dump_interval_key_name);
        if (!restart_dump_dirname_key_name.empty())
        {
            d_restart_dump_dirname = main_db->getString(restart_dump_dirname_key_name);
            if (d_restart_dump_dirname.empty())
            {
                pout << "WARNING: AppInitializer::AppInitializer(): " << restart_dump_interval_key_name << " > 0, but `"
                     << d_restart_dump_dirname << "' is empty\n";
            }
        }
        else
        {
            pout << "WARNING: AppInitializer::AppInitializer(): " << restart_dump_interval_key_name
                 << " > 0, but `restart_dump_dirname' is not specified in input file\n";
        }
    }

    // Configure post-processing data output options.
    std::string data_dump_interval_key_name;
    if (main_db->keyExists("data_interval"))
    {
        data_dump_interval_key_name = "data_interval";
    }
    else if (main_db->keyExists("data_dump_interval"))
    {
        data_dump_interval_key_name = "data_dump_interval";
    }
    else if (main_db->keyExists("data_write_interval"))
    {
        data_dump_interval_key_name = "data_write_interval";
    }

    std::string data_dump_dirname_key_name;
    if (main_db->keyExists("data_dirname"))
    {
        data_dump_dirname_key_name = "data_dirname";
    }
    else if (main_db->keyExists("data_dump_dirname"))
    {
        data_dump_dirname_key_name = "data_dump_dirname";
    }
    else if (main_db->keyExists("data_write_dirname"))
    {
        data_dump_dirname_key_name = "data_write_dirname";
    }

    if (!data_dump_interval_key_name.empty())
    {
        d_data_dump_interval = main_db->getInteger(data_dump_interval_key_name);
        if (!data_dump_dirname_key_name.empty())
        {
            d_data_dump_dirname = main_db->getString(data_dump_dirname_key_name);
            if (d_data_dump_dirname.empty())
            {
                pout << "WARNING: AppInitializer::AppInitializer(): " << data_dump_interval_key_name << " > 0, but `"
                     << d_data_dump_dirname << "' is empty\n";
            }
        }
        else
        {
            pout << "WARNING: AppInitializer::AppInitializer(): " << data_dump_interval_key_name
                 << " > 0, but `data_dump_dirname' is not specified in input file\n";
        }
    }
    return;
} // AppInitializer

AppInitializer::~AppInitializer()
{
    InputManager::freeManager();
    return;
} // ~AppInitializer

Pointer<Database>
AppInitializer::getInputDatabase()
{
    return d_input_db;
} // getInputDatabase

bool
AppInitializer::isFromRestart() const
{
    return d_is_from_restart;
} // isFromRestart

const std::string&
AppInitializer::getRestartReadDirectory() const
{
    return d_restart_read_dirname;
}

int
AppInitializer::getRestartRestoreNumber() const
{
    return d_restart_restore_num;
}

Pointer<Database>
AppInitializer::getRestartDatabase(const bool suppress_warning)
{
    if (!d_is_from_restart && !suppress_warning)
    {
        pout << "WARNING: AppInitializer::getRestartDatabase(): Not a restarted run, restart "
                "database is empty\n";
    }
    return RestartManager::getManager()->getRootDatabase();
} // getRestartDatabase

Pointer<Database>
AppInitializer::getComponentDatabase(const std::string& component_name, const bool suppress_warning)
{
    const bool db_exists = d_input_db->isDatabase(component_name);
    if (!db_exists && !suppress_warning)
    {
        pout << "WARNING: AppInitializer::getComponentDatabase(): Database corresponding to "
                "component `"
             << component_name << "' not found in input\n";
        return new NullDatabase();
    }
    else
    {
        return d_input_db->getDatabase(component_name);
    }
} // getComponentDatabase

bool
AppInitializer::dumpVizData() const
{
    return d_viz_dump_interval > 0;
} // dumpVizData

int
AppInitializer::getVizDumpInterval() const
{
    return d_viz_dump_interval;
} // getVizDumpInterval

std::string
AppInitializer::getVizDumpDirectory() const
{
    return d_viz_dump_dirname;
} // getVizDumpDirectory

std::vector<std::string>
AppInitializer::getVizWriters() const
{
    return d_viz_writers;
} // getVizDumpDirectory

Pointer<VisItDataWriter<NDIM>>
AppInitializer::getVisItDataWriter() const
{
    return d_visit_data_writer;
} // getVisItDataWriter

Pointer<LSiloDataWriter>
AppInitializer::getLSiloDataWriter() const
{
    return d_silo_data_writer;
} // getLSiloDataWriter

std::string
AppInitializer::getExodusIIFilename(const std::string& prefix) const
{
    std::string exodus_filename;
    if (!d_exodus_filename.empty())
    {
        exodus_filename = d_viz_dump_dirname + "/" + prefix + d_exodus_filename;
    }
    return exodus_filename;
} // getExodusIIFilename

std::string
AppInitializer::getGMVFilename(const std::string& prefix) const
{
    std::string gmv_filename;
    if (!d_gmv_filename.empty())
    {
        gmv_filename = d_viz_dump_dirname + "/" + prefix + d_gmv_filename;
    }
    return gmv_filename;
} // getGMVFilename

bool
AppInitializer::dumpRestartData() const
{
    return d_restart_dump_interval > 0;
} // dumpRestartData

int
AppInitializer::getRestartDumpInterval() const
{
    return d_restart_dump_interval;
} // getRestartDumpInterval

std::string
AppInitializer::getRestartDumpDirectory() const
{
    return d_restart_dump_dirname;
} // getRestartDumpDirectory

bool
AppInitializer::dumpPostProcessingData() const
{
    return d_data_dump_interval > 0;
} // dumpPostProcessingData

int
AppInitializer::getPostProcessingDataDumpInterval() const
{
    return d_data_dump_interval;
} // getPostProcessingDataDumpInterval

std::string
AppInitializer::getPostProcessingDataDumpDirectory() const
{
    return d_data_dump_dirname;
} // getPostProcessingDataDumpDirectory

bool
AppInitializer::dumpTimerData() const
{
    return d_timer_dump_interval > 0;
} // dumpTimerData

int
AppInitializer::getTimerDumpInterval() const
{
    return d_timer_dump_interval;
} // getTimerDumpInterval

/////////////////////////////// PROTECTED ////////////////////////////////////

/////////////////////////////// PRIVATE //////////////////////////////////////

/////////////////////////////// NAMESPACE ////////////////////////////////////

} // namespace IBTK

//////////////////////////////////////////////////////////////////////////////
