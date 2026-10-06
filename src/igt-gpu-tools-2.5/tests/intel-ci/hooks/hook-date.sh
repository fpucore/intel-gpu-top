#!/bin/bash

# Example hook which stores current date in <testname>/current-date.txt
# in attachments dir
#
# Suggested usage with:
# --hook 'post-subtest:tests/intel-ci/hooks/hook-date.sh' \
# --hook 'post-dyn-subtest:tests/intel-ci/hooks/hook-date.sh'
# or within hooks-wrapper.sh

ALLOWLIST="hook-date.allowlist"
SCRIPTDIR=$(dirname "$(realpath "$0")")
. "${SCRIPTDIR}/common.helper.sh"

cd "${IGT_RUNNER_ATTACHMENTS_DIR}"

mkdir -p "${IGT_HOOK_TEST_FULLNAME}"
date > "${IGT_HOOK_TEST_FULLNAME}/current-date.txt"
