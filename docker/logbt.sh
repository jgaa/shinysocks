#!/usr/bin/env bash

set -eu
set -o pipefail
# This setting enables the desired behavior in the "for corefile in ${core_directory}/${SEARCH_PATTERN_NON_TRACKED}; do" loop
# more details at https://gist.github.com/springmeyer/6dd234ff89ba306a73608a6f45cb5506
shopt -s nullglob

PLATFORM_UNAME=$(uname -s)
REQUIRED_FILENAME="core"
LOGBT_VERSION="v3.0.0"
BASE_CORE_DIRECTORY=/tmp/logbt-coredumps
READ_ONLY_MODE=false
STRICT_MODE=false

if [[ ${PLATFORM_UNAME} == "Linux" ]]; then
  REQUIRED_PATTERN="${REQUIRED_FILENAME}.%p.%E"
  DEBUGGER="gdb"
  DEBUGGER_COMMAND="thread apply all bt"
elif [[ ${PLATFORM_UNAME} == "Darwin" ]]; then
  # Recommend running with the following setting to only show crashes
  # in the notification center
  # defaults write com.apple.CrashReporter UseUNC 1
  REQUIRED_PATTERN="${REQUIRED_FILENAME}.%P"
  DEBUGGER="lldb"
  DEBUGGER_COMMAND="thread backtrace all"
else
  error "Error: Unsupported platform: ${PLATFORM_UNAME}"
fi

function error() {
  >&2 echo "[logbt] $*"
  exit 1
}

function process_core() {
  local program=${1}
  local corefile=${2}
  local debugger=${3}
  echo "debug command: ${DEBUGGER_COMMAND}"
  if [[ ${debugger} =~ "lldb" ]]; then
    lldb --core "${corefile}" --batch -o "${DEBUGGER_COMMAND}" -o "quit"
  else
    gdb "${program}" --core "${corefile}" -ex "set print thread-events off" -ex "set pagination 0" -ex "${DEBUGGER_COMMAND}" --batch
  fi
  # note: on OS X the -f avoids a hang on prompt "remove write-protected regular file?"
  if [[ ${READ_ONLY_MODE} == false ]]; then
    rm -f "${corefile}"
  fi
}

function find_core_by_pid() {
  local program=${1}
  local core_directory=${2}
  local debugger=${3}
  local child_pid=${4}
  if [[ ${PLATFORM_UNAME} == "Darwin" ]]; then
    local single_corefile="${core_directory}/${REQUIRED_FILENAME}.${child_pid}"
    if [ -e "${single_corefile}" ]; then
      echo "[logbt] Found corefile at ${single_corefile}"
      process_core "${program}" "${single_corefile}" "${debugger}"
    fi
  else
    local SEARCH_PATTERN_BY_PID="${REQUIRED_FILENAME}.${child_pid}.*"
    # note: this for loop depends on the `shopt -s nullglob` above
    for corefile in ${core_directory}/${SEARCH_PATTERN_BY_PID}; do
      echo "[logbt] Found corefile at ${corefile}"
      # extract program name from corefile
      filename=$(basename "${corefile}")
      binary_program=/$(echo "${filename##*.\!}" | tr "!" "/")
      process_core "${binary_program}" "${corefile}" "${debugger}"
    done
  fi
}

function find_remaining_cores() {
  local program=${1}
  local core_directory=${2}
  local debugger=${3}
  local SEARCH_PATTERN_NON_TRACKED="${REQUIRED_FILENAME}.*"
  local hit=false
  for corefile in ${core_directory}/${SEARCH_PATTERN_NON_TRACKED}; do
    echo "[logbt] Found corefile (non-tracked) at ${corefile}"
    hit=true
  done
  if [[ ${hit} == true ]]; then
    echo "[logbt] Processing cores..."
  fi
  for corefile in ${core_directory}/${SEARCH_PATTERN_NON_TRACKED}; do
    # below two lines are linux specific, but harmless to run on osx
    filename=$(basename "${corefile}")
    binary_program=/$(echo "${filename##*.\!}" | tr "!" "/")
    process_core "${binary_program}" "${corefile}" "${debugger}"
  done
}

function snapshot {
  local program=${1}
  local debugger=${2}
  local child_pid=${3}
  echo "[logbt] snapshotting ${program} (${child_pid})"
  if [[ ${debugger} =~ "lldb" ]]; then
    lldb -p "${child_pid}" --batch -o "thread backtrace all" -o "quit"
  else
    gdb --pid "${child_pid}" -ex "set pagination 0" -ex "thread apply all bt" --batch
  fi
}

function backtrace {
  local program=${1}
  local core_directory=${2}
  local debugger=${3}
  local child_pid=${4}
  find_core_by_pid "${program}" "${core_directory}" "${debugger}" "${child_pid}"
  find_remaining_cores "${program}" "${core_directory}" "${debugger}"
}

function warn_on_existing_cores() {
  local core_directory=${1}
  local SEARCH_PATTERN_NON_TRACKED="${REQUIRED_FILENAME}.*"
  for corefile in ${core_directory}/${SEARCH_PATTERN_NON_TRACKED}; do
    echo "[logbt] WARNING: Found corefile (existing) at ${corefile}"
  done
}

function error_on_existing_cores() {
  local core_directory=${1}
  local SEARCH_PATTERN_NON_TRACKED="${REQUIRED_FILENAME}.*"
  for corefile in ${core_directory}/${SEARCH_PATTERN_NON_TRACKED}; do
    if [[ ${STRICT_MODE} == true ]]; then
      error "Error: Found corefile (unexpected) at ${corefile}"
    else
      echo "[logbt] WARNING: Found corefile (unexpected) at ${corefile}"
    fi
  done
}

function ensure_directory_is_writeable() {
  # ensure we can write to the directory, otherwise
  # core files might not be able to be written
  WRITE_RETURN=0
  touch "${core_directory}"/test.txt || WRITE_RETURN=$?
  if [[ ${WRITE_RETURN} != 0 ]]; then
    error "Error: Permissions problem: unable to write to ${core_directory} (exited with ${WRITE_RETURN})"
  else
    # cleanup from test
    rm "${core_directory}"/test.txt
  fi
}

function get_target_core_pattern() {
  echo ${BASE_CORE_DIRECTORY}/${REQUIRED_PATTERN}
}

function get_core_pattern() {
  if [[ ${PLATFORM_UNAME} == "Linux" ]]; then
    local core_pattern
    core_pattern=$(cat /proc/sys/kernel/core_pattern)
  elif [[ ${PLATFORM_UNAME} == "Darwin" ]]; then
    # Recommend running with the following setting to only show crashes
    # in the notification center
    # defaults write com.apple.CrashReporter UseUNC 1
    local core_pattern
    core_pattern=$(sysctl -n kern.corefile)
  fi
  echo "${core_pattern}"
}

function validate_core_pattern() {
  local core_pattern=${1}
  if [[ ! ${core_pattern} =~ ${REQUIRED_PATTERN} ]]; then
    if [[ ${STRICT_MODE} == true ]]; then
      error "Error: unexpected core_pattern: ${core_pattern}"
    else
      echo "[logbt] WARNING: unexpected core_pattern: ${core_pattern}"
    fi

  fi
}

function generic_signal_handler() {
  local code=$?
  local program=${1}
  local child_pid=${2}
  local sig=${3}
  # Bug note: On darwin ${code} will be incorrectly 0 after snapshot here
  # so we ignore ${code} and instead get it from the signal
  code=$(($(kill -l "${sig}")+128))
  echo "[logbt] received signal:${code} (${sig})"
  echo "[logbt] sending SIGTERM to ${program} (${child_pid})"
  # sleep here to help the stdout show in the right
  # order (accounts for an intermittant case where the above lines print after
  # the child outputs stdout during shutdown)
  sleep 1
  KILL_RETURN=0
  kill -TERM "${child_pid}" || KILL_RETURN=$?
  if [[ ${KILL_RETURN} != 0 ]]; then
    echo "[logbt] could not terminate child process (kill returned ${KILL_RETURN})"
  else
    CHILD_EXIT=0
    wait "${child_pid}" || CHILD_EXIT=$?
    if [[ ${CHILD_EXIT} != 143 ]]; then
      error "Error: child process exited abnormally: ${CHILD_EXIT}"
    fi
  fi
  echo "[logbt] exiting with ${code}"
  exit ${code}
}


: '
NOTE: SIGINT (aka ctrl-c) is special. First of all SIGINT is sent to the whole process group
automatically in bash (http://stackoverflow.com/a/6804155). This means we do not need to reap
the child process manually. And if logbt is a "foreground" process then SIGINT is ignored if
sent directly with ./bin/logbt <args> & kill -INT $! (http://stackoverflow.com/a/14697034).
So the only way to send SIGINT is with ctrl-c or via another terminal that has a different
process group.
'

function sigint_handler() {
  local code=$?
  local program=${1}
  local child_pid=${2}
  local sig=${3}
  echo "[logbt] received signal:${code} (${sig})"
  CHILD_EXIT=0
  wait "${child_pid}" || CHILD_EXIT=$?
  if [[ ${CHILD_EXIT} != 130 ]]; then
    echo "[logbt] child process exited with:${CHILD_EXIT}"
  fi
  exit ${code}
}

function launch_and_wait() {

  local program=${1}
  local core_pattern
  core_pattern=$(get_core_pattern)
  validate_core_pattern "${core_pattern}"

  local core_directory
  core_directory=$(dirname "${core_pattern}")
  echo "[logbt] using corefile location: ${core_directory}"
  echo "[logbt] using core_pattern: $(basename "${core_pattern}")"

  # ensure we have a debugger installed
  if ! command -v "${DEBUGGER}" > /dev/null; then
    error "Error: Could not find required command '${DEBUGGER}'"
  fi

  if [[ ! -d ${core_directory} ]] && [[ ${READ_ONLY_MODE} == false ]]; then
    echo "[logbt] creating directory for core files at '${core_directory}'"
    mkdir -p "${core_directory}"
    chmod a+w "${core_directory}"
  fi

  if [[ ${READ_ONLY_MODE} == false ]]; then
    ensure_directory_is_writeable "${core_directory}"
  fi

  warn_on_existing_cores "${core_directory}"

  # Enable corefile generation
  ulimit -c unlimited

  # Run the child process in a background process
  # in order to get the PID
  if [[ -n "${LD_PRELOAD:-}" ]] && [[ "${PLATFORM_UNAME}" == 'Darwin' ]]; then
      # on os x DYLD_INSERT_LIBRARIES is blocked from being inherited
      # so we accept LD_PRELOAD and foreward along
      DYLD_INSERT_LIBRARIES=${LD_PRELOAD} LOGBT_PID=$$ "$@" & CHILD_PID=$!
  else
      LOGBT_PID=$$ "$@" & CHILD_PID=$!
  fi

  # Hook up function to run when logbt received signal
  trap "snapshot ${program} ${DEBUGGER} ${CHILD_PID}" USR1
  trap "generic_signal_handler ${program} ${CHILD_PID} TERM" TERM
  trap "generic_signal_handler ${program} ${CHILD_PID} HUP" HUP
  trap "sigint_handler ${program} ${CHILD_PID} INT" INT

  # Wait for child and attempt to generate a backtrace if child exits in non-zero way
  wait_for_child "${program}" "${core_directory}" "${DEBUGGER}" "${CHILD_PID}"
}

function wait_for_child() {
  local program=${1}
  local core_directory=${2}
  local debugger=${3}
  local child_pid=${4}
  CHILD_RETURN=0
  wait "${child_pid}" || CHILD_RETURN=$?
  if [[ ${CHILD_RETURN} == 127 ]]; then
    # command not found : http://www.tldp.org/LDP/abs/html/exitcodes.html
    echo "[logbt] command not found: ${program}"
  elif [[ ${CHILD_RETURN} != 0 ]]; then
    # On linux (and recent OS X) USR1 will trigger an exit which makes it look like the child
    # has returned when it has not. So the below code ensures we stay alive in order to watch
    # the child for when USR1 is hit
    if [[ $(kill -l ${CHILD_RETURN}) == USR1 ]]; then
      wait_for_child "${program}" "${core_directory}" "${debugger}" "${child_pid}"
    fi
    local exit_msg="saw '${program}' exit with code:${CHILD_RETURN}"
    exit_msg="${exit_msg} ($(kill -l ${CHILD_RETURN}))"
    echo "[logbt] ${exit_msg}"
    backtrace "${program}" "${core_directory}" "${DEBUGGER}" "${child_pid}" "${CHILD_RETURN}"
  fi
  # exit logbt with the same code as the child
  exit ${CHILD_RETURN}
}

function setup_logbt() {
  if [[ ${READ_ONLY_MODE} == true ]]; then
    error "Error: --setup and --read-only-mode cannot be used together"
  fi
  local settable_core_pattern
  settable_core_pattern=$(get_target_core_pattern)
  if [[ ${PLATFORM_UNAME} == "Linux" ]]; then
    echo "[logbt] setting $(cat /proc/sys/kernel/core_pattern) -> ${settable_core_pattern}"
    # write new value to /proc/sys/kernel/core_pattern
    echo "${settable_core_pattern}" > /proc/sys/kernel/core_pattern
  elif [[ ${PLATFORM_UNAME} == "Darwin" ]]; then
    echo "[logbt] setting $(sysctl -n kern.corefile) -> ${settable_core_pattern}"
    sysctl kern.corefile="${settable_core_pattern}"
  fi

  local core_pattern
  core_pattern=$(get_core_pattern)
  validate_core_pattern "${core_pattern}"
  local core_directory
  core_directory=$(dirname "${core_pattern}")

  if [[ ! -d ${core_directory} ]]; then
    echo "[logbt] creating directory for core files at '${core_directory}'"
    mkdir -p "${core_directory}"
    chmod a+w "${core_directory}"
  fi

  error_on_existing_cores "${core_directory}"

}

function set_read_only_mode() {
  READ_ONLY_MODE=true
}

function set_strict_mode() {
  STRICT_MODE=true
}

function set_debug_command() {
  DEBUGGER_COMMAND=${1}
}

function test_logbt() {
  if [[ ${READ_ONLY_MODE} == true ]]; then
    error "Error: --test and --read-only-mode cannot be used together"
  fi
  ulimit -c unlimited
  # First we create a program that crashes itself
  # We use bash to avoid needing an external dep on some runtime

  # Due to https://github.com/mapbox/logbt/issues/29 we need to copy the bash
  # exe on OS X to a new location since coredumps are disabled for /bin/bash for
  # reasons I don't understand
  if [[ ${PLATFORM_UNAME} == "Darwin" ]]; then
      # first touch file to create it with writable permissions for this user
      # such that we can cleanup after
      # then copy the system bash there
      cp "$(command -v bash)" /tmp/tmp-bash
      chmod +x /tmp/tmp-bash
      echo '#!/tmp/tmp-bash' > /tmp/crasher.sh
  else
    # on linux the default bash is okay
      echo '#!/usr/bin/env bash' > /tmp/crasher.sh
  fi
  echo 'kill -SIGSEGV $$' >> /tmp/crasher.sh
  chmod +x /tmp/crasher.sh
  # run it in logbt
  RETURN=0
  "${BASH_SOURCE[0]}" -- /tmp/crasher.sh >/tmp/logbt-stdout 2>/tmp/logbt-stderr || RETURN=$?
  local err_message
  if [[ ${RETURN} != 139 ]] || [[ ! $(cat /tmp/logbt-stdout) =~ "Found corefile at" ]]; then
    cat /tmp/logbt-stdout
    cat /tmp/logbt-stderr
    err_message="Expected return code of 139 and a corefile to be generated"
  fi
  if [[ ! $(cat /tmp/logbt-stdout) =~ "Found corefile at" ]]; then
    cat /tmp/logbt-stdout
    cat /tmp/logbt-stderr
    err_message="Expected a corefile to be generated"
  fi
  # cleanup
  rm -f /tmp/logbt-stderr
  rm -f /tmp/logbt-stdout
  rm -f /tmp/crasher.sh
  rm -f /tmp/tmp-bash
  if [[ -n ${err_message:-} ]]; then
    error "Error: ${err_message}"
  else
    echo "[logbt] test success (coredumps are working with core pattern: '$(get_core_pattern)')"
  fi
}

function usage() {
  >&2 echo "Usage for logbt:"
  >&2 echo ""
  >&2 echo "Setup logbt (requires root privileges):"
  >&2 echo ""
  >&2 echo "$ sudo logbt --setup"
  >&2 echo ""
  >&2 echo "Test logbt is setup correctly"
  >&2 echo ""
  >&2 echo "$ logbt --test"
  >&2 echo ""
  >&2 echo "Launch a program with logbt:"
  >&2 echo ""
  >&2 echo "$ logbt -- ./program"
  >&2 echo ""
  >&2 echo "Other commands are:"
  >&2 echo ""
  >&2 echo " --current-pattern"
  >&2 echo " --target-pattern"
  >&2 echo " --read-only-mode"
  >&2 echo " --debug-command \"thread apply all bt full\""
  exit 1
}

function get_version() {
  echo ${LOGBT_VERSION}
}

if [[ -z ${1:-} ]]; then
  usage
fi

# https://stackoverflow.com/questions/192249/how-do-i-parse-command-line-arguments-in-bash
for i in "$@"
do
case $i in
    --)
    if [[ -z ${2:-} ]]; then
      usage
    fi
    shift
    launch_and_wait "$@"
    ;;
    --strict-mode)
    set_strict_mode
    shift
    ;;
    --read-only-mode)
    set_read_only_mode
    shift
    ;;
    --setup)
    setup_logbt
    shift
    ;;
    --debug-command)
    if [[ -z ${2:-} ]]; then
      usage
    fi
    shift
    set_debug_command "$@"
    shift
    ;;
    --test)
    test_logbt
    shift
    ;;
    --current-pattern)
    get_core_pattern
    ;;
    --target-pattern)
    get_target_core_pattern
    ;;
    -v | --version)
    get_version
    shift
    ;;
    -h | --help)
    usage
    shift
    ;;
    *)
    ;;
esac
done
