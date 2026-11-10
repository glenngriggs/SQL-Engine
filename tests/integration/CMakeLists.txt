# Golden tests: every cases/<name>.sql is run in verbose and quiet mode and its stdout compared
# byte for byte with cases/<name>.out and cases/<name>.quiet.out. Commands run from the project
# root so LOAD paths in the scripts are relative to it.

set(RUN_CASE ${CMAKE_CURRENT_SOURCE_DIR}/run_case.cmake)
set(CASES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cases)
set(CLI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cli)

set(ACTUAL_DIR ${CMAKE_CURRENT_BINARY_DIR}/actual)
file(MAKE_DIRECTORY ${ACTUAL_DIR})

function(sillyql_add_case_test name)
  add_test(NAME ${name}
    COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:silly> -DACTUAL_DIR=${ACTUAL_DIR} ${ARGN}
            -P ${RUN_CASE}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endfunction()

function(sillyql_add_golden name input)
  sillyql_add_case_test(golden.${name}.verbose
    -DINPUT=${input} -DEXPECTED_STDOUT=${CASES_DIR}/${name}.out)
  sillyql_add_case_test(golden.${name}.quiet
    -DINPUT=${input} -DARGS=-q -DEXPECTED_STDOUT=${CASES_DIR}/${name}.quiet.out)
endfunction()

file(GLOB case_inputs CONFIGURE_DEPENDS ${CASES_DIR}/*.sql)
foreach(input IN LISTS case_inputs)
  get_filename_component(name ${input} NAME_WE)
  sillyql_add_golden(${name} ${input})
endforeach()

# The market-data example doubles as a golden test; its expected output lives with the others.
sillyql_add_golden(market_demo ${PROJECT_SOURCE_DIR}/examples/market_demo.sql)

# Command-line behaviour: flags, exit status and stderr.
set(usage_error "-DEXPECTED_STDERR=Error: Unknown command line option")
set(input_error "-DEXPECTED_STDERR=Error: Reading from cin has failed")

sillyql_add_case_test(cli.unknown_short_flag
  -DINPUT=${CASES_DIR}/spec.sql -DARGS=-x -DEXPECTED_EXIT=1 ${usage_error}
  -DEXPECTED_STDOUT=${CLI_DIR}/no_output.out)
sillyql_add_case_test(cli.unknown_long_flag
  -DINPUT=${CASES_DIR}/spec.sql -DARGS=--verbose -DEXPECTED_EXIT=1 ${usage_error}
  -DEXPECTED_STDOUT=${CLI_DIR}/no_output.out)
sillyql_add_case_test(cli.positional_argument
  -DINPUT=${CASES_DIR}/spec.sql -DARGS=spec.sql -DEXPECTED_EXIT=1 ${usage_error}
  -DEXPECTED_STDOUT=${CLI_DIR}/no_output.out)
sillyql_add_case_test(cli.help_short
  -DINPUT=${CLI_DIR}/empty.sql -DARGS=-h "-DSTDOUT_CONTAINS=Usage: ")
sillyql_add_case_test(cli.help_long
  -DINPUT=${CLI_DIR}/empty.sql -DARGS=--help "-DSTDOUT_CONTAINS=--timing")
sillyql_add_case_test(cli.help_stops_option_parsing
  -DINPUT=${CLI_DIR}/empty.sql "-DARGS=-h -x" "-DSTDOUT_CONTAINS=Usage: ")
sillyql_add_case_test(cli.long_option_prefix
  -DINPUT=${CASES_DIR}/spec.sql -DARGS=--qui -DEXPECTED_STDOUT=${CASES_DIR}/spec.quiet.out)
sillyql_add_case_test(cli.end_of_options
  -DINPUT=${CASES_DIR}/spec.sql "-DARGS=-q --" -DEXPECTED_STDOUT=${CASES_DIR}/spec.quiet.out)
sillyql_add_case_test(cli.quiet_long_flag
  -DINPUT=${CASES_DIR}/spec.sql -DARGS=--quiet -DEXPECTED_STDOUT=${CASES_DIR}/spec.quiet.out)
sillyql_add_case_test(cli.timing_writes_only_to_stderr
  -DINPUT=${CASES_DIR}/spec.sql -DARGS=-t -DEXPECTED_STDOUT=${CASES_DIR}/spec.out
  "-DSTDERR_LINE_REGEX=^\\[timing\\] [A-Z]+ [0-9]+ us$")
sillyql_add_case_test(cli.combined_flags
  -DINPUT=${CASES_DIR}/join.sql -DARGS=-qt -DEXPECTED_STDOUT=${CASES_DIR}/join.quiet.out
  "-DSTDERR_LINE_REGEX=^\\[timing\\] [A-Z]+ [0-9]+ us$")
sillyql_add_case_test(cli.empty_input
  -DINPUT=${CLI_DIR}/empty.sql -DEXPECTED_EXIT=1 ${input_error}
  -DEXPECTED_STDOUT=${CLI_DIR}/empty.out)
sillyql_add_case_test(cli.eof_before_quit
  -DINPUT=${CLI_DIR}/eof_before_quit.sql -DEXPECTED_EXIT=1 ${input_error}
  -DEXPECTED_STDOUT=${CLI_DIR}/eof_before_quit.out)
sillyql_add_case_test(cli.malformed_number
  -DINPUT=${CLI_DIR}/malformed_number.sql -DEXPECTED_EXIT=1 ${input_error}
  -DEXPECTED_STDOUT=${CLI_DIR}/malformed_number.out)
