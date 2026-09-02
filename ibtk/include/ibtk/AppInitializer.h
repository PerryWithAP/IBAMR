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

/////////////////////////////// INCLUDE GUARD ////////////////////////////////

#ifndef included_IBTK_AppInitializer
#define included_IBTK_AppInitializer

/////////////////////////////// INCLUDES /////////////////////////////////////

#include <ibtk/config.h>

#include <ibtk/LSiloDataWriter.h>

#include <tbox/Database.h>
#include <tbox/DescribedClass.h>
#include <tbox/Pointer.h>

#include <VisItDataWriter.h>

#include <string>
#include <vector>

/////////////////////////////// CLASS DEFINITION /////////////////////////////

namespace IBTK
{
/*!
 * \brief Class AppInitializer provides functionality to simplify the
 * initialization code in an application code.
 *
 * Ordinary PETSc options used by calls to `Petsc*SetFromOptions()` made after
 * construction may be supplied by flat `petsc_settings` or
 * `petsc_settings_<tag>` databases anywhere in the input tree. An untagged
 * leaf key `k` produces `-k`; a tagged key produces `-<tag>_<k>`.
 * Tags start with an ASCII letter, contain letters, digits, or internal
 * underscores, and end with a letter or digit. Case and internal underscores
 * are preserved. Block locations and existing solver-prefix keys do not
 * contribute prefixes or change a solver's configured prefix.
 *
 * Values must be single-element Booleans, integers, floats, doubles, or
 * nonempty strings; floating-point values must be finite. Singleton arrays
 * cannot be distinguished from scalars. Settings blocks must be flat.
 * Expanded names must be valid PETSc names of at most 255 bytes including
 * the leading dash. Case-insensitive duplicate names across all blocks are
 * errors, even when their values agree. The entire collection is validated
 * before insertion.
 *
 * Alternatively, the root keys `PETSC_OPTIONS_FILE` and `petsc_options_file`
 * select a legacy file, with uppercase precedence. The file is sought as
 * named, then relative to the canonical input-file directory. Nested file
 * keys are ignored. Root file keys and settings blocks, including empty
 * blocks, are mutually exclusive. Existing `petsc_options_*` entries retain
 * their meanings and are not recognized as settings.
 *
 * Inline values do not affect PETSc initialization-time behavior. Existing
 * ordinary PETSc options take precedence. Testing for an existing option with
 * `PetscOptionsHasName()` marks that option as used and can affect
 * `-options_left` reporting. PETSc aliases affecting these keys and an active
 * global options-prefix stack are unsupported.
 *
 * Inline settings supply ordinary option values, not PETSc parser controls.
 * Known file-source and prefix-push/pop controls are rejected; other
 * version-specific parser controls are outside this interface.
 * Legacy mode instead asks PETSc to parse a file and reprocess local copies
 * of the original command line, so later ordinary command-line values win.
 * It does not clear the existing options database or repeat initialization.
 * File-source controls, aliases, prefix-stack state, and unusual positional
 * arguments resembling PETSc keys are outside that precedence guarantee.
 */
class AppInitializer : public SAMRAI::tbox::DescribedClass
{
public:
    /*!
     * Constructor for class AppInitializer parses command line arguments, sets
     * up input and restart databases, and enables SAMRAI logging.
     */
    AppInitializer(int argc, char* argv[], const std::string& default_log_file_name = "IBAMR.log");

    /*!
     * Destructor for class AppInitializer frees the SAMRAI manager objects
     * used to set up input and restart databases.
     */
    virtual ~AppInitializer();

    /*!
     * Return a pointer to the input database.
     */
    SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> getInputDatabase();

    /*!
     * Return a boolean value indicating whether this is a restarted run.
     * This is defined as true when the program recognizes command line arguments
     * for restart_restore_num and restart_dump_dirname.
     */
    bool isFromRestart() const;

    /*!
     * Return the restart directory.  If we are not starting from restart,
     * this method returns an empty string. This value is set by the second
     * command line argument to the executable.
     */
    const std::string& getRestartReadDirectory() const;

    /*!
     * Return the restart restore number.  If we are not starting from restart,
     * this method returns 0. This value is set by the third command line
     * argument to the executable.
     */
    int getRestartRestoreNumber() const;

    /*!
     * Return a pointer to the restart database.  If there is no restart
     * database for the application, this method emits a warning message and
     * returns a NullDatabse.
     */
    SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> getRestartDatabase(bool suppress_warning = false);

    /*!
     * Return initialization database for the requested solver component.  This
     * is equivalent to:
     * getInputDatabase()->getDatabase(component_name).
     *
     * If the requested component is not found in the input database, this
     * method emits a warning message and returns a NullDatabse.
     */
    SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> getComponentDatabase(const std::string& component_name,
                                                                       bool suppress_warning = false);

    /*!
     * Return a boolean value indicating whether to write visualization data.
     */
    bool dumpVizData() const;

    /*!
     * Return the visualization dump interval.
     */
    int getVizDumpInterval() const;

    /*!
     * Return the visualization dump directory name.
     */
    std::string getVizDumpDirectory() const;

    /*!
     * Return the visualization writers to be used in the simulation.
     */
    std::vector<std::string> getVizWriters() const;

    /*!
     * Return a VisIt data writer object to be used to output Cartesian grid
     * data.
     *
     * If the application is not configured to use VisIt, a nullptr pointer will be
     * returned.
     */
    SAMRAI::tbox::Pointer<SAMRAI::appu::VisItDataWriter<NDIM>> getVisItDataWriter() const;

    /*!
     * Return a VisIt data writer object to be used to output Lagrangian data.
     *
     * If the application is not configured to use VisIt, a nullptr pointer will be
     * returned.
     */
    SAMRAI::tbox::Pointer<LSiloDataWriter> getLSiloDataWriter() const;

    /*!
     * Return the ExodusII visualization file name.
     *
     * If the application is not configured to use ExodusII, an empty string
     * will be returned.
     */
    std::string getExodusIIFilename(const std::string& prefix = "") const;

    /*!
     * Return the GMV visualization file name.
     *
     * If the application is not configured to use GMV, an empty string
     * will be returned.
     */
    std::string getGMVFilename(const std::string& prefix = "") const;

    /*!
     * Return a boolean value indicating whether to write restart data.
     */
    bool dumpRestartData() const;

    /*!
     * Return the restart dump interval. This is set in the Main database of
     * the input file. This can be defined in the input file as restart_interval,
     * restart_dump_interval, or restart_write_interval.
     */
    int getRestartDumpInterval() const;

    /*!
     * Return the restart dump directory name. This is set in the Main database
     * of the input file. This can be defined in the input file as restart_dump_dirname,
     * restart_dirname, or restart_write_dirname.
     */
    std::string getRestartDumpDirectory() const;

    /*!
     * Return a boolean value indicating whether to write post processing data.
     */
    bool dumpPostProcessingData() const;

    /*!
     * Return the post processing data dump interval.
     */
    int getPostProcessingDataDumpInterval() const;

    /*!
     * Return the post processing data dump directory name.
     */
    std::string getPostProcessingDataDumpDirectory() const;

    /*!
     * Return a boolean value indicating whether to write timer data.
     */
    bool dumpTimerData() const;

    /*!
     * Return the timer dump interval.
     */
    int getTimerDumpInterval() const;

private:
    // Throws std::invalid_argument before insertion if any settings definition is invalid.
    static void insertPetscSettings(SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> input_db);

    /*!
     * \brief Copy constructor.
     *
     * \note This constructor is not implemented and should not be used.
     *
     * \param from The value to copy to this object.
     */
    AppInitializer(const AppInitializer& from) = delete;

    /*!
     * \brief Assignment operator.
     *
     * \note This operator is not implemented and should not be used.
     *
     * \param that The value to assign to this object.
     *
     * \return A reference to this object.
     */
    AppInitializer& operator=(const AppInitializer& that) = delete;

    /*!
     * The input database.
     */
    SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> d_input_db;

    /*!
     * Restart settings.
     */
    std::string d_restart_read_dirname;
    int d_restart_restore_num = 0;
    bool d_is_from_restart = false;

    /*!
     * Visualization options.
     */
    int d_viz_dump_interval = 0;
    std::string d_viz_dump_dirname;
    std::vector<std::string> d_viz_writers;
    SAMRAI::tbox::Pointer<SAMRAI::appu::VisItDataWriter<NDIM>> d_visit_data_writer;
    SAMRAI::tbox::Pointer<LSiloDataWriter> d_silo_data_writer;
    std::string d_exodus_filename = "output.ex2", d_gmv_filename = "output.gmv";

    /*!
     * Restart options.
     */
    int d_restart_dump_interval = 0;
    std::string d_restart_dump_dirname;

    /*!
     * Post-processing options.
     */
    int d_data_dump_interval = 0;
    std::string d_data_dump_dirname;

    /*!
     * Timer options.
     */
    int d_timer_dump_interval = 0;
};
} // namespace IBTK

//////////////////////////////////////////////////////////////////////////////

#endif // #ifndef included_IBTK_AppInitializer
