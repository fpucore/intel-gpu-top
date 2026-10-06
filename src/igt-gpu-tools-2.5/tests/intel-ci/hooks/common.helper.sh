# Common helper for hooks executed from igt_runner
#
# Usually ALLOWLIST=<filename> is set in the hook, then this helper is
# sourced allowing arg parsing and being overwritten by --allowlist.

for arg in "$@"; do
	case $arg in
	--help)
		echo "Usage: $0 [--help] [--allowlist <name.allowlist>]"
		echo
		echo "If name.allowlist is not passed default '${ALLOWLIST}' is used."
		exit 0
		;;

	--allowlist)
		ALLOWLIST=$2
		shift 2
		;;

		*)
		shift
		;;
	esac
done

if [ "${ALLOWLIST}" == "" ]; then
	echo "No ALLOWLIST was provided. Exiting..."
	exit 0
fi

if [ -z "${IGT_RUNNER_ATTACHMENTS_DIR}" ]; then
	echo "Missing IGT_RUNNER_ATTACHMENTS_DIR env"
	exit 0
fi

# Look for allowlist in following places:
# 1. Try in IGT_HOOK_ALLOWLIST_DIR if this environment exists
# 2. Try in <SCRIPTDIR>

if [ ! -z "${IGT_HOOK_ALLOWLIST_DIR}" ]; then
	ALLOWLIST_PATH="${IGT_HOOK_ALLOWLIST_DIR}/${ALLOWLIST}"
else
	ALLOWLIST_PATH="${SCRIPTDIR}/${ALLOWLIST}"
fi

if [ ! -e "${ALLOWLIST_PATH}" ]; then
	echo "Missing ${ALLOWLIST_PATH}"
	exit 0
fi

if ! echo "${IGT_HOOK_TEST_FULLNAME}" | grep -q -f "$ALLOWLIST_PATH"; then
	exit 0
fi
