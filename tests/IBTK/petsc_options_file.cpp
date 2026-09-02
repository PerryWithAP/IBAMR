// ---------------------------------------------------------------------
//
// Copyright (c) 2026 by the IBAMR developers
// All rights reserved.
//
// This file is part of IBAMR.
//
// IBAMR is free software and is distributed under the 3-clause BSD
// license. The full text of the license can be found in the file
// COPYRIGHT at the top level directory of IBAMR.
//
// ---------------------------------------------------------------------

#include <ibtk/AppInitializer.h>
#include <ibtk/IBTKInit.h>
#include <ibtk/IBTK_CHKERRQ.h>
#include <ibtk/IBTK_MPI.h>

#include <tbox/Database.h>
#include <tbox/Logger.h>
#include <tbox/Utilities.h>

#include <petscoptions.h>
#include <petscsys.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <ibamr/app_namespaces.h>

namespace
{
bool
has_option(const std::string& name)
{
    PetscBool present = PETSC_FALSE;
    int ierr = PetscOptionsHasName(nullptr, nullptr, name.c_str(), &present);
    IBTK_CHKERRQ(ierr);
    return present;
}

void
verify_values(Pointer<Database> expected)
{
    const Array<std::string> keys = expected->getAllKeys();
    int failures = 0;
    for (int k = 0; k < keys.size(); ++k)
    {
        const std::string name = "-" + keys[k];
        PetscBool present = PETSC_FALSE;
        int ierr = 0;
        switch (expected->getArrayType(keys[k]))
        {
        case Database::SAMRAI_BOOL:
        {
            PetscBool value = PETSC_FALSE;
            ierr = PetscOptionsGetBool(nullptr, nullptr, name.c_str(), &value, &present);
            failures += value != expected->getBool(keys[k]);
            break;
        }
        case Database::SAMRAI_INT:
        {
            PetscInt value = 0;
            ierr = PetscOptionsGetInt(nullptr, nullptr, name.c_str(), &value, &present);
            failures += value != expected->getInteger(keys[k]);
            break;
        }
        case Database::SAMRAI_DOUBLE:
        {
            PetscReal value = 0.0;
            ierr = PetscOptionsGetReal(nullptr, nullptr, name.c_str(), &value, &present);
            failures += value != expected->getDouble(keys[k]);
            break;
        }
        case Database::SAMRAI_STRING:
        {
            char value[512] = {};
            ierr = PetscOptionsGetString(nullptr, nullptr, name.c_str(), value, sizeof(value), &present);
            failures += value != expected->getString(keys[k]);
            break;
        }
        default:
            TBOX_ERROR("unsupported expected type\n");
        }
        IBTK_CHKERRQ(ierr);
        failures += !present;
    }
    if (IBTK_MPI::sumReduction(failures)) TBOX_ERROR("PETSc settings value mismatch on one or more ranks\n");
}

// Record the real abort message, without volatile source-file/line metadata.
class OptionsErrorAppender : public Logger::Appender
{
public:
    explicit OptionsErrorAppender(bool check_sentinel) : d_check_sentinel(check_sentinel)
    {
    }

    void logMessage(const std::string& message, const std::string&, const int) override
    {
        if (IBTK_MPI::getRank() != 0) return;
        std::ofstream output("output");
        if (d_check_sentinel)
        {
            PetscBool present = PETSC_FALSE;
            const int ierr = PetscOptionsHasName(nullptr, nullptr, "-r00_atomic_marker", &present);
            // Do not recursively invoke the abort logger on a failed query.
            if (ierr)
            {
                output << "sentinel query failed\n" << std::flush;
                return;
            }
            output << "sentinel " << (present ? "present" : "absent") << '\n';
        }
        // SAMRAI appends a NUL: use c_str() as in TestAppender.
        output << message.c_str() << std::flush;
    }

private:
    bool d_check_sentinel;
};
} // namespace

int
main(int argc, char* argv[])
{
    if (argc < 2) return 2;
    // Leave input symlinks for AppInitializer to resolve.
    const std::filesystem::path input_path = std::filesystem::absolute(argv[1]);
    unsetenv("PETSC_OPTIONS");
    unsetenv("PETSC_OPTIONS_YAML");
    unsetenv("PETSC_OPTIONS_YAML_DIRECTORY");

    std::vector<std::string> storage = { argv[0], input_path.string(), "-skip_petscrc" };
    if (input_path.string().find(".lowercase.") != std::string::npos)
        storage.insert(storage.end(), { "-r00_value", "4404" });
    if (input_path.string().find(".inline.") != std::string::npos)
        storage.insert(storage.end(), { "-SP_cli_value", "6606" });
    std::vector<char*> args;
    for (std::string& value : storage) args.push_back(value.data());
    args.push_back(nullptr);
    std::vector<char*> init_args = args;
    IBTKInit ibtk_init(static_cast<int>(storage.size()), init_args.data(), MPI_COMM_WORLD);
    if (input_path.string().find(".inline.") != std::string::npos)
    {
        int ierr = PetscOptionsSetValue(nullptr, "-SP_programmatic_value", "7707");
        IBTK_CHKERRQ(ierr);
    }
    Pointer<Logger::Appender> abort_appender =
        new OptionsErrorAppender(input_path.string().find(".invalid_later.") != std::string::npos);
    Logger::getInstance()->setAbortAppender(abort_appender);

    Pointer<AppInitializer> initializer =
        new AppInitializer(static_cast<int>(storage.size()), args.data(), "petsc_options_file.log");
    // Normal continuation must return success, so attest rejects an expected-error case.
    if (input_path.string().find(".expect_error=true.") != std::string::npos) return 0;
    Pointer<Database> input = initializer->getInputDatabase();
    if (input->isDatabase("Expected")) verify_values(input->getDatabase("Expected"));
    if (input->keyExists("absent"))
    {
        const Array<std::string> absent = input->getStringArray("absent");
        for (int k = 0; k < absent.size(); ++k)
            if (has_option("-" + absent[k])) TBOX_ERROR("ignored metadata inserted " << absent[k] << "\n");
    }
    if (IBTK_MPI::getRank() == 0)
    {
        std::ofstream output("output");
        output << "PETSc options verified\n";
    }
    return 0;
}
