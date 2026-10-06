#!/bin/bash

# Hooks wrapper
# Suggested usage with:
# --hook 'post-subtest:tests/intel-ci/hooks/hooks-wrapper.sh' \
# --hook 'post-dyn-subtest:tests/intel-ci/hooks/hooks-wrapper.sh'

SCRIPTDIR=$(dirname "$(realpath "$0")")
cd "${SCRIPTDIR}"

# Example hook which stores current date
#./hook-date.sh

# Copy GuC logs for tests which are failing and are in the allowlist
./copy_guc_log_on_fail.sh
