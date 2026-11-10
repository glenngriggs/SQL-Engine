# Runs the silly executable on one input and checks its stdout, stderr and exit status.
#
#   cmake -DEXE=<silly> -DINPUT=<file> [-DARGS="<flags>"] [-DEXPECTED_STDOUT=<file>]
#         [-DEXPECTED_EXIT=<code>] [-DEXPECTED_STDERR=<line>] [-DSTDERR_LINE_REGEX=<regex>]
#         [-DSTDOUT_CONTAINS=<text>] [-DACTUAL_DIR=<dir>] -P run_case.cmake
#
# EXPECTED_STDOUT is compared byte for byte; on a mismatch the actual output is saved in
# ACTUAL_DIR (when given) for diffing. EXPECTED_STDERR is the exact single line that
# stderr must contain. With STDERR_LINE_REGEX, stderr must be non-empty and every line must match.

foreach(required EXE INPUT)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "run_case.cmake: ${required} is not set")
  endif()
endforeach()
if(NOT DEFINED EXPECTED_EXIT)
  set(EXPECTED_EXIT 0)
endif()
separate_arguments(arg_list UNIX_COMMAND "${ARGS}")

execute_process(
  COMMAND ${EXE} ${arg_list}
  INPUT_FILE ${INPUT}
  OUTPUT_VARIABLE actual_stdout
  ERROR_VARIABLE actual_stderr
  RESULT_VARIABLE actual_exit)

set(failures "")

if(NOT actual_exit STREQUAL EXPECTED_EXIT)
  string(APPEND failures "exit status ${actual_exit}, expected ${EXPECTED_EXIT}\n")
endif()

if(DEFINED EXPECTED_STDOUT)
  file(READ ${EXPECTED_STDOUT} expected_stdout)
  if(NOT actual_stdout STREQUAL expected_stdout)
    string(APPEND failures "stdout differs from ${EXPECTED_STDOUT}\n")
    if(DEFINED ACTUAL_DIR)
      get_filename_component(case_name ${EXPECTED_STDOUT} NAME)
      set(actual_file ${ACTUAL_DIR}/${case_name})
      file(WRITE ${actual_file} "${actual_stdout}")
      string(APPEND failures "  actual output saved to ${actual_file}\n")
    endif()
  endif()
endif()

if(DEFINED STDOUT_CONTAINS)
  string(FIND "${actual_stdout}" "${STDOUT_CONTAINS}" position)
  if(position EQUAL -1)
    string(APPEND failures "stdout does not contain '${STDOUT_CONTAINS}'\n")
  endif()
endif()

if(DEFINED EXPECTED_STDERR)
  if(NOT actual_stderr STREQUAL "${EXPECTED_STDERR}\n")
    string(APPEND failures "stderr was '${actual_stderr}', expected '${EXPECTED_STDERR}'\n")
  endif()
elseif(DEFINED STDERR_LINE_REGEX)
  string(REGEX REPLACE "\n$" "" stderr_body "${actual_stderr}")
  if(stderr_body STREQUAL "")
    string(APPEND failures "stderr is empty\n")
  endif()
  string(REPLACE "\n" ";" stderr_lines "${stderr_body}")
  foreach(stderr_line IN LISTS stderr_lines)
    if(NOT stderr_line MATCHES "${STDERR_LINE_REGEX}")
      string(APPEND failures "stderr line '${stderr_line}' does not match ${STDERR_LINE_REGEX}\n")
    endif()
  endforeach()
elseif(NOT actual_stderr STREQUAL "")
  string(APPEND failures "unexpected stderr: ${actual_stderr}\n")
endif()

if(NOT failures STREQUAL "")
  message(FATAL_ERROR "${failures}")
endif()
