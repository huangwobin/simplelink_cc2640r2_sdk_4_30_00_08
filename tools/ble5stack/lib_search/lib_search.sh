#!/bin/bash

main() {
    set -x
    LIB_SEARCH_SCRIPT_DIR="$( cd "$( dirname "${0}" )" && pwd )"

    UTILITY_SCRIPT="$LIB_SEARCH_SCRIPT_DIR"/../../../tools/ble5stack/utility/utility.sh
    if [ -e "$UTILITY_SCRIPT" ]; then
        # Source utility script
        . "$UTILITY_SCRIPT"

        # Detect OS
        detect_os

        # Detect ble repo path
        detect_ble_root

        # Find local Python
        detect_python_path

        # Run lib_search python script
        if [[ ! -z "$PYTHON_SANDBOX_PATH" ]]; then
            "$PYTHON_SANDBOX_PATH" "$LIB_SEARCH_SCRIPT_DIR"/lib_search.py "$@"
        fi
    else
        # Run lib_gen executable
        "$LIB_SEARCH_SCRIPT_DIR"/../lib_gen/lib_gen.exe "$@"
    fi
}

main "$@"
