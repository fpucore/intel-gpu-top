#!/bin/bash

# Hook script for copying guc.log.
# Suggested usage with:
# --hook 'post-subtest:tests/intel-ci/hooks/copy_guc_log_on_fail.sh' \
# --hook 'post-dyn-subtest:tests/intel-ci/hooks/copy_guc_log_on_fail.sh'
# or within hooks-wrapper.sh

# Copy only for failed subtests as this is time-consuming
if [ "${IGT_HOOK_RESULT:-FAIL}" != "FAIL" ]; then
	exit 0
fi

ALLOWLIST="copy_guc_log_on_fail.allowlist"
SCRIPTDIR=$(dirname "$(realpath "$0")")
. "${SCRIPTDIR}/common.helper.sh"

cd "${IGT_RUNNER_ATTACHMENTS_DIR}"

for log in $(find /sys/kernel/debug/dri -regextype posix-egrep -iregex '.*(guc_log|guc_info)'); do
	attout=$(echo ${log:23} | sed -e 's/\//_/g')
	mkdir -p "${IGT_HOOK_TEST_FULLNAME}"
	cp "$log" "${IGT_HOOK_TEST_FULLNAME}/${attout}"
done
