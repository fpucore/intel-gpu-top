// SPDX-License-Identifier: MIT
/*
 * Copyright © 2026 Google
 *
 * Authors:
 *   Louis Chauvet <louis.chauvet@bootlin.com>
 */

#include <asm-generic/errno-base.h>
#include <stdint.h>

#include "igt_core.h"

#include "unigraf.h"
#include "TSI.h"
#include "TSI_types.h"
#include "igt_aux.h"
#include "igt_edid.h"
#include "igt_kms.h"
#include "igt_pipe_crc.h"
#include "igt_rc.h"
#include "monitor_edids/monitor_edids_helper.h"

#define unigraf_debug(fmt, ...)	igt_debug("TSI:%p: " fmt, unigraf_device, ##__VA_ARGS__)

static TSI_HANDLE unigraf_device;
static char *unigraf_default_edid;
static char *unigraf_connector_name;
static bool unigraf_crc;

/**
 * UNIGRAF_NAME_MAX - Maximum name length to be used for TSI functions
 */
#define UNIGRAF_NAME_MAX 1024

/**
 * UNIGRAF_CONFIG_GROUP - Name of the unigraf group in the configuration file
 */
#define UNIGRAF_CONFIG_GROUP "Unigraf"

/**
 * UNIGRAF_CONFIG_DEVICE_NAME - Key of the device name in the configuration file
 */
#define UNIGRAF_CONFIG_DEVICE_NAME "Device"

/**
 * UNIGRAF_CONFIG_DEVICE_ROLE - Key of the device role in the configuration file
 */
#define UNIGRAF_CONFIG_DEVICE_ROLE "Role"

/**
 * UNIGRAF_CONFIG_INPUT_NAME - Key of the input name in the configuration file
 */
#define UNIGRAF_CONFIG_INPUT_NAME "Input"

/**
 * UNIGRAF_CONFIG_CONNECTOR_NAME - Key of the connector name in the configuration file
 */
#define UNIGRAF_CONFIG_CONNECTOR_NAME "Connector"

/**
 * UNIGRAF_CONFIG_EDID_NAME - Key of the EDID name in the configuration file
 */
#define UNIGRAF_CONFIG_EDID_NAME "EDID"

/**
 * UNIGRAF_CONFIG_USE_CRC_NAME - Key of the CRC selection in the configuration file
 */
#define UNIGRAF_CONFIG_USE_CRC_NAME "UseCRC"

/**
 * UNIGRAF_CONFIG_MST_STREAM_COUNT - Key for the stream count configuration
 *
 * Set to 0 to use SST, 1..4 for MST
 */
#define UNIGRAF_CONFIG_MST_STREAM_COUNT "MSTStreams"

/**
 * UNIGRAF_DEFAULT_ROLE_NAME - Default role name to search on the unigraf device
 */
#define UNIGRAF_DEFAULT_ROLE_NAME "USB-C, DP Alt Mode Source and Sink"

/**
 * UNIGRAF_DEFAULT_INPUT_NAME - Default input name to search on the unigraf device
 */
#define UNIGRAF_DEFAULT_INPUT_NAME "DP RX"

/**
 * UNIGRAF_EDID_MAX_SIZE - Max EDID size that can be read from the unigraf. The
 */
#define UNIGRAF_EDID_MAX_SIZE 2048

static void unigraf_close_device(void)
{
	if (!unigraf_device)
		return;

	unigraf_debug("Closing...\n");
	unigraf_assert(TSIX_DEV_CloseDevice(unigraf_device));
	TSI_Clean();
	unigraf_device = NULL;
	free(unigraf_default_edid);
	free(unigraf_connector_name);
}

/**
 * unigraf_exit_handler - Handle the exit signal and clean up unigraf resources.
 * @sig: The signal number received.
 *
 * This function is called when the program receives an exit signal. It ensures
 * that all unigraf resources are properly cleaned up by calling unigraf_deinit
 * for each open instance.
 */
static void unigraf_exit_handler(int sig)
{
	unigraf_close_device();
}

static void unigraf_init(void)
{
	int ret;

	unigraf_debug("Initialize unigraf...\n");
	ret = TSI_Init(TSI_CURRENT_VERSION);
	unigraf_assert(ret);
	igt_install_exit_handler(unigraf_exit_handler);
}

/**
 * unigraf_write_u32 - Write a 32-bit value to a TSI configuration item
 * @config_id: The configuration item ID to write to
 * @value: The 32-bit value to write
 *
 * This macro writes a 32-bit value to the specified TSI configuration item.
 * This is a macro to have the proper line information when using unigraf_debug.
 */
#define unigraf_write_u32(config_id, value)									\
	({													\
		uint32_t v = (value);										\
		igt_assert(unigraf_device);									\
		unigraf_debug("Value write: " #config_id "=%d...\n", v);					\
		unigraf_assert(TSIX_TS_SetConfigItem(unigraf_device, config_id, &v, sizeof(value)));		\
	})

/**
 * unigraf_read_u32 - Read a 32-bit value from a TSI configuration item with error handling
 * @config_id: The configuration item ID to read from
 *
 * This macro reads a 32-bit value from the specified TSI configuration item.
 * It includes error handling and debug output. This is a macro to have the proper
 * line information when using unigraf_debug.
 *
 * Returns: The 32-bit value read from the configuration item
 */
#define unigraf_read_u32(config_id)										\
	({													\
		uint32_t value;											\
		igt_assert(unigraf_device);									\
		unigraf_assert(TSIX_TS_GetConfigItem(unigraf_device, config_id, &value, sizeof(value)));	\
		unigraf_debug("Value read: " #config_id "=%d\n", value);					\
		value;												\
	})

/**
 * unigraf_write() - Helper to write a value to unigraf
 * @config: config id to write
 * @data: data to write
 * @data_len: length of the data
 */
static void unigraf_write(TSI_CONFIG_ID config, const void *data, size_t data_len)
{
	unigraf_debug("Writing %zu bytes to 0x%x\n", data_len, config);

	unigraf_assert(TSIX_TS_SetConfigItem(unigraf_device, config, data, data_len));
}

/**
 * unigraf_device_count() - Return the number of scanned devices
 *
 * Must be called after a unigraf_rescan_devices().
 */
static unsigned int unigraf_device_count(void)
{
	return unigraf_assert(TSIX_DEV_GetDeviceCount());
}

static int unigraf_find_device(char *request)
{
	int device_count = unigraf_device_count();

	for (int i = 0; i < device_count; i++) {
		char dev_name[UNIGRAF_NAME_MAX] = "";

		unigraf_assert(TSIX_DEV_GetDeviceName(i, dev_name, UNIGRAF_NAME_MAX));
		unigraf_debug("Detected unigraf device %d: %s\n", i, dev_name);
		if (!strncmp(dev_name, request, UNIGRAF_NAME_MAX))
			return i;
	}
	return -ENODEV;
}

static int unigraf_find_role(const char *request)
{
	int role_count = unigraf_assert(TSIX_DEV_GetDeviceRoleCount(unigraf_device));

	for (int i = 0; i < role_count; i++) {
		char role_name[UNIGRAF_NAME_MAX] = "";

		unigraf_assert(TSIX_DEV_GetDeviceRoleName(unigraf_device, i,
							  role_name,
							  UNIGRAF_NAME_MAX));
		unigraf_debug("Role %d: %s\n", i, role_name);
		if (!strncmp(role_name, request, UNIGRAF_NAME_MAX))
			return i;
	}
	return -ENODEV;
}

static int unigraf_find_input(const char *request)
{
	int role_count = unigraf_assert(TSIX_VIN_GetInputCount(unigraf_device));

	for (int i = 0; i < role_count; i++) {
		char input_name[UNIGRAF_NAME_MAX] = "";

		unigraf_assert(TSIX_VIN_GetInputName(unigraf_device, i,
						     input_name, UNIGRAF_NAME_MAX));
		unigraf_debug("Input %d: %s\n", i, input_name);
		if (!strncmp(input_name, request, UNIGRAF_NAME_MAX))
			return i;
	}
	return -ENODEV;
}

static void unigraf_load_default_edid(void)
{
	struct edid *edid = get_edid_by_name(unigraf_default_edid);

	if (!edid) {
		igt_warn("Impossible to find an edid named \"%s\"", unigraf_default_edid);
		list_edid_names(IGT_LOG_WARN);
		return;
	}

	for (int i = unigraf_get_mst_stream_max_count(); i > 0; i--)
		unigraf_write_edid(i - 1, edid, edid_get_size(edid));
}

static void unigraf_autodetect_connector(int drm_fd)
{
	int newly_connected_count, already_connected_count, diff_len;
	uint32_t *newly_connected = NULL, *already_connected = NULL;
	drmModeConnectorPtr connector;
	uint32_t *diff = NULL;

	igt_assert_fd(drm_fd);
	unigraf_set_sst();
	unigraf_hpd_deassert();

	/*
	 * Hard sleep is required here as we don't know how long it will take for the device under
	 * test to properly detect the port disconnection.
	 */
	sleep(igt_default_display_detect_timeout());

	already_connected_count = igt_get_connected_connectors(drm_fd, &already_connected);

	unigraf_hpd_assert();

	newly_connected_count = kms_wait_for_new_connectors(&newly_connected,
							    already_connected,
							    already_connected_count,
							    drm_fd);

	diff_len = get_array_diff(newly_connected, newly_connected_count,
				  already_connected, already_connected_count, &diff);

	if (!diff_len) {
		unigraf_debug("No newly connected connector, assuming that the unigraf is not connected.\n");
	} else if (diff_len > 1) {
		unigraf_debug("More than one new connectors connected, this is not supported by autodetection.\n");
	} else {
		unigraf_debug("Found one connector (%d) connected to the Unigraf\n", diff[0]);
		connector = drmModeGetConnector(drm_fd, diff[0]);
		igt_assert(connector);
		igt_assert(asprintf(&unigraf_connector_name, "%s-%u",
				    kmstest_connector_type_str(connector->connector_type),
				    connector->connector_type_id) != -1);
		drmModeFreeConnector(connector);
	}

	free(already_connected);
	free(newly_connected);
	free(diff);
}

/**
 * unigraf_get_connector() - Get a DRM connector for the Unigraf device
 * @drm_fd: DRM file descriptor
 *
 * Returns: A pointer to the drmModeConnector connected to the unigraf device,
 * or NULL if the operation failed. The caller is responsible for freeing this
 * pointer with drmModeFreeConnector().
 */
drmModeConnectorPtr unigraf_get_connector(int drm_fd)
{
	if (!unigraf_connector_name)
		unigraf_autodetect_connector(drm_fd);
	return igt_get_connector_from_name(drm_fd, unigraf_connector_name);
}

/**
 * unigraf_get_connector_id_by_stream() - Get a connector id from a stream id
 * @drm_fd: DRM device where the unigraf is attached
 * @stream_id: Stream id in the MST streams
 */
int unigraf_get_connector_id_by_stream(int drm_fd, int stream_id)
{
	drmModeConnectorPtr main_connector = unigraf_get_connector(drm_fd);
	drmModeResPtr res = drmModeGetResources(drm_fd);

	char *mst_path;
	int mst_path_len = asprintf(&mst_path, "mst:%d-", main_connector->connector_id);

	igt_assert(mst_path_len >= 0);

	for (int i = 0; i < res->count_connectors; i++) {
		drmModePropertyBlobPtr path_blob = kmstest_get_path_blob(drm_fd,
									 res->connectors[i]);

		if (!path_blob || path_blob->length < mst_path_len)
			continue;

		if (!memcmp(path_blob->data, mst_path, mst_path_len)) {
			if (!stream_id) {
				drmModeFreePropertyBlob(path_blob);
				drmModeFreeResources(res);
				drmModeFreeConnector(main_connector);
				return res->connectors[i];
			}
			stream_id--;
		}

		drmModeFreePropertyBlob(path_blob);
	}

	drmModeFreeResources(res);
	drmModeFreeConnector(main_connector);

	return 0;
}

/**
 * unigraf_open_device() - Search and open a device.
 * @drm_fd: File descriptor of the currently used drm device
 *
 * Assert if:
 * - called outside tests / fixup
 * - TSI library raise errors
 * Returns: true if a device was found and initialized, otherwise false.
 *
 * This function searches for a compatible device and opens it.
 */
bool unigraf_open_device(int drm_fd)
{
	TSI_RESULT r;
	GError *cfg_error = NULL;
	char *cfg_device = NULL;
	char *cfg_role = NULL;
	char *cfg_input = NULL;
	char *cfg_edid_name = NULL;
	int device_count;
	int chosen_device = 0;
	int chosen_role;
	int chosen_input;
	int unigraf_stream_count;

	assert(igt_can_fail());

	if (unigraf_device)
		return unigraf_connector_name != NULL;

	unigraf_init();

	if (igt_key_file) {
		cfg_device = g_key_file_get_string(igt_key_file, UNIGRAF_CONFIG_GROUP,
						   UNIGRAF_CONFIG_DEVICE_NAME, &cfg_error);
		if (cfg_error) {
			unigraf_debug("No device name configured, uses first device available.\n");
			cfg_device = NULL;
		}

		cfg_error = NULL;
		cfg_role = g_key_file_get_string(igt_key_file, UNIGRAF_CONFIG_GROUP,
						 UNIGRAF_CONFIG_DEVICE_ROLE, &cfg_error);
		if (cfg_error) {
			unigraf_debug("No device role configured.\n");
			cfg_role = NULL;
		}

		cfg_error = NULL;
		cfg_input = g_key_file_get_string(igt_key_file, UNIGRAF_CONFIG_GROUP,
						  UNIGRAF_CONFIG_INPUT_NAME, &cfg_error);
		if (cfg_error) {
			unigraf_debug("No input name configured.\n");
			cfg_input = NULL;
		}

		cfg_error = NULL;
		unigraf_connector_name = g_key_file_get_string(igt_key_file, UNIGRAF_CONFIG_GROUP,
							       UNIGRAF_CONFIG_CONNECTOR_NAME,
							       &cfg_error);
		if (cfg_error) {
			unigraf_debug("No connector name configured, will autodetect.\n");
			unigraf_connector_name = NULL;
		}

		cfg_error = NULL;
		cfg_edid_name = g_key_file_get_string(igt_key_file, UNIGRAF_CONFIG_GROUP,
						      UNIGRAF_CONFIG_EDID_NAME, &cfg_error);
		if (cfg_error) {
			unigraf_debug("No default EDID set, use IGT default.\n");
			cfg_edid_name = NULL;
		}

		cfg_error = NULL;
		unigraf_crc = g_key_file_get_boolean(igt_key_file, UNIGRAF_CONFIG_GROUP,
						     UNIGRAF_CONFIG_USE_CRC_NAME, &cfg_error);
		if (cfg_error) {
			unigraf_debug("CRC usage not configured, using unigraf CRC.\n");
			unigraf_crc = true;
		}

		cfg_error = NULL;
		unigraf_stream_count = g_key_file_get_integer(igt_key_file, UNIGRAF_CONFIG_GROUP,
							      UNIGRAF_CONFIG_MST_STREAM_COUNT,
							      &cfg_error);
		if (cfg_error) {
			unigraf_debug("MST usage not configured, using SST.\n");
			unigraf_stream_count = 0;
		}
	}

	unigraf_assert(TSIX_DEV_RescanDevices(0, TSI_DEVCAP_VIDEO_CAPTURE, 0));

	device_count = unigraf_device_count();
	if (device_count < 1) {
		unigraf_debug("No device found.\n");
		return false;
	}

	if (cfg_device) {
		chosen_device = unigraf_find_device(cfg_device);
		if (chosen_device < 0) {
			igt_warn("The requested unigraf device %s is not found, err: %d.\n", cfg_device, chosen_device);
			return false;
		}
	}

	unigraf_device = TSIX_DEV_OpenDevice(chosen_device, &r);
	unigraf_assert(r);
	igt_assert(unigraf_device);
	unigraf_debug("Successfully opened the unigraf device %d.\n", chosen_device);

	if (!cfg_role) {
		unigraf_debug("No role configured, trying " UNIGRAF_DEFAULT_ROLE_NAME "\n");
		chosen_role = unigraf_find_role(UNIGRAF_DEFAULT_ROLE_NAME);
		if (chosen_role < 0) {
			char role_name[UNIGRAF_NAME_MAX];

			chosen_role = 0;
			unigraf_assert(TSIX_DEV_GetDeviceRoleName(unigraf_device, chosen_role,
								  role_name, UNIGRAF_NAME_MAX));
			unigraf_debug("Role " UNIGRAF_DEFAULT_ROLE_NAME " not found, using role 0 (%s)\n",
				      role_name);
		}
	} else {
		chosen_role = unigraf_find_role(cfg_role);
		igt_assert_f(chosen_role >= 0, "TSI:%p: Role %s not found.",
			     unigraf_device, cfg_role);
	}

	unigraf_assert(TSIX_DEV_SelectRole(unigraf_device, chosen_role));

	if (!cfg_input) {
		unigraf_debug("No input configured, trying " UNIGRAF_DEFAULT_INPUT_NAME "\n");
		chosen_input = unigraf_find_input(UNIGRAF_DEFAULT_INPUT_NAME);
		if (chosen_input < 0) {
			char input_name[UNIGRAF_NAME_MAX];

			chosen_input = 0;
			unigraf_assert(TSIX_VIN_GetInputName(unigraf_device, chosen_input,
							     input_name, UNIGRAF_NAME_MAX));
			unigraf_debug("Input " UNIGRAF_DEFAULT_INPUT_NAME " not found, using input 0 (%s).\n",
				      input_name);
		}
	} else  {
		chosen_input = unigraf_find_input(cfg_input);
		igt_assert_f(chosen_input >= 0, "TSI:%p: Input %s not found.",
			     unigraf_device, cfg_input);
	}

	unigraf_assert(TSIX_VIN_Select(unigraf_device, chosen_input));
	unigraf_assert(TSIX_VIN_Enable(unigraf_device, chosen_input));

	if (!cfg_edid_name)
		cfg_edid_name = strdup("DEL_16543_DELL_P2314T_DP");

	unigraf_default_edid = cfg_edid_name;

	if (!unigraf_connector_name) {
		unigraf_hpd_deassert();
		unigraf_set_sst();
		unigraf_hpd_assert();
		unigraf_autodetect_connector(drm_fd);
	}

	unigraf_reset();

	if (!unigraf_stream_count)
		unigraf_set_sst();
	else
		unigraf_set_mst_stream_count(unigraf_stream_count);

	return unigraf_connector_name != NULL;
}

/**
 * unigraf_require_device() - Search and open a device.
 * @drm_fd: File descriptor of the currently used drm device
 *
 * This is a shorthand to reduce test boilerplate when a unigraf device must be present.
 */
void unigraf_require_device(int drm_fd)
{
	igt_require(unigraf_open_device(drm_fd));
}

/**
 * unigraf_reset() - Reset the Unigraf device
 *
 * This function performs a hardware reset of the Unigraf device, restoring it to a
 * default state. This includes resetting all configuration parameters, stream settings,
 * and link parameters to default values.
 */
void unigraf_reset(void)
{
	unigraf_plug();
	unigraf_set_mst_stream_count(1);
	unigraf_set_sst();
	unigraf_load_default_edid();
	unigraf_hpd_assert();
	unigraf_set_max_lane_count(4);
	unigraf_set_max_link_rate(UNIGRAF_RATE_8_10_GHZ);
}

/**
 * unigraf_read_edid() - Read the EDID from the specified stream
 * @stream: The stream ID to read the EDID from
 * @edid_size: Pointer to an integer where the size of the EDID will be stored
 *
 * Returns: A pointer to the EDID structure, or NULL if the operation failed. The caller
 * is responsible to free this pointer.
 */
struct edid *unigraf_read_edid(uint32_t stream, uint32_t *edid_size)
{
	void *edid;

	unigraf_debug("Read EDID for stream %d...\n", stream);

	edid = calloc(UNIGRAF_EDID_MAX_SIZE, sizeof(char));

	unigraf_write_u32(TSI_EDID_SELECT_STREAM, stream);
	*edid_size = unigraf_assert(TSIX_TS_GetConfigItem(unigraf_device,
							  TSI_EDID_TE_INPUT,
							  edid, UNIGRAF_EDID_MAX_SIZE));

	return edid;
}

/**
 * unigraf_write_edid() - Write EDID data to the specified stream
 * @stream: The stream ID to write the EDID to
 * @edid: Pointer to the EDID structure to write
 * @edid_size: Size of the EDID data in bytes
 *
 * This function writes the provided EDID data to the specified stream.
 */
void unigraf_write_edid(uint32_t stream, const struct edid *edid, uint32_t edid_size)
{
	unigraf_debug("Write EDID for stream %d...\n", stream);

	unigraf_write_u32(TSI_EDID_SELECT_STREAM, stream);
	unigraf_write(TSI_EDID_TE_INPUT, edid, min(edid_size, (uint32_t)UNIGRAF_EDID_MAX_SIZE));
}

/**
 * unigraf_hpd_assert() - Assert Hot Plug Detect signal
 *
 * This function asserts the HPD signal, simulating a device connection.
 */
void unigraf_hpd_assert(void)
{
	unigraf_write_u32(TSI_FORCE_HOT_PLUG_STATE_W, 1);
}

/**
 * unigraf_hpd_pulse() - Pulse the Hot Plug Detect signal
 * @duration: The duration in milliseconds for which the HPD signal should be pulsed
 *
 * This function pulses the HPD signal for the specified duration.
 */
void unigraf_hpd_pulse(int duration)
{
	/* In theory this should work:
	 * unigraf_write_u32(TSI_DPRX_HPD_PULSE_W, duration);
	 * But this seems to be broken and this works:
	 */
	unigraf_hpd_deassert();
	usleep(duration);
	unigraf_hpd_assert();
}

/**
 * unigraf_hpd_deassert() - Deassert Hot Plug Detect signal
 *
 * This function deasserts the HPD signal, simulating a device disconnection.
 */
void unigraf_hpd_deassert(void)
{
	unigraf_write_u32(TSI_FORCE_HOT_PLUG_STATE_W, 0);
}

/**
 * unigraf_plug() - Emulate a cable unplug
 *
 * This function will emulate a full cable unplug (not a simple HPD line change)
 */
void unigraf_unplug(void)
{
	int d = 2 << 2;

	unigraf_write_u32(TSI_DPRX_HPD_FORCE, d);
}

/**
 * unigraf_plug() - Emulate a cable plug
 *
 * This function will emulate a full cable plug (not a simple HPD line change)
 */
void unigraf_plug(void)
{
	int d = 3 << 2;

	unigraf_write_u32(TSI_DPRX_HPD_FORCE, d);
}

/**
 * unigraf_set_sst() - Configure the device for Single Stream Transport mode
 *
 * This function sets the device to operate in Single Stream Transport (SST) mode.
 */
void unigraf_set_sst(void)
{
	int link_flags = 0xFFFFFFFF;

	unigraf_assert(TSIX_TS_GetConfigItem(unigraf_device, TSI_DPRX_LINK_FLAGS,
					     &link_flags, sizeof(link_flags)));
	link_flags &= ~(TSI_DPRX_LINK_FLAGS_MST | TSI_DPRX_NOT_DOCUMENTED_SIDEBAND_MSG_SUPPORT);
	unigraf_write_u32(TSI_DPRX_LINK_FLAGS, link_flags);
}

/**
 * unigraf_set_mst() - Configure the device for Multi Stream Transport mode
 *
 * This function sets the device to operate in Multi Stream Transport (MST) mode.
 */
void unigraf_set_mst(void)
{
	int link_flags = 0xFFFFFFFF;

	unigraf_assert(TSIX_TS_GetConfigItem(unigraf_device, TSI_DPRX_LINK_FLAGS,
					     &link_flags, sizeof(link_flags)));
	link_flags |= TSI_DPRX_LINK_FLAGS_MST | TSI_DPRX_NOT_DOCUMENTED_SIDEBAND_MSG_SUPPORT;
	unigraf_write_u32(TSI_DPRX_LINK_FLAGS, link_flags);
}

/**
 * unigraf_get_mst_stream_max_count() - Get the maximum number of stream count accepted by the
 * device
 * Caution: This function can be destructive to some configuration: the only way to get the
 * information is to try and read the new value.
 */
int unigraf_get_mst_stream_max_count(void)
{
	struct TSI_DPRX_HW_CAPS_R_s caps;

	unigraf_assert(TSIX_TS_GetConfigItem(unigraf_device, TSI_DPRX_HW_CAPS_R,
					     &caps, sizeof(caps)));

	return caps.mst_stream_count;
}

/**
 * unigraf_get_mst_stream_count() - Get the current number of MST streams
 *
 * Returns: The current number of MST streams configured on the device.
 */
int unigraf_get_mst_stream_count(void)
{
	return unigraf_read_u32(TSI_DPRX_MST_SINK_COUNT);
}

/**
 * unigraf_set_mst_stream_count() - Set the number of accepted stream count
 *
 * Returns true when the stream count was properly applied, false if the final stream count
 * is not the one requested
 */
bool unigraf_set_mst_stream_count(int count)
{
	int new_count;

	igt_assert_lte(count, unigraf_get_mst_stream_max_count());

	unigraf_write_u32(TSI_DPRX_MST_SINK_COUNT, count);
	new_count = unigraf_get_mst_stream_count();

	igt_warn_on_f(count != new_count,
		      "IGT:%p: Requested MST stream count (%d) differs from what was applied by the device (%d)\n",
		      unigraf_device, count, new_count);

	return count == new_count;
}

/**
 * unigraf_select_stream() - Select the active stream for the device
 * @stream: The stream index to select
 *
 * This function selects the active stream for the device. The stream index
 * should be a valid stream number that the device supports.
 */
void unigraf_select_stream(int stream)
{
	unigraf_write_u32(TSI_DPRX_STREAM_SELECT, stream);
}

/**
 * unigraf_read_crc() - Read the CRC values from the Unigraf device
 * @stream: Stream to grab the CRC from
 * @out: Pointer to an igt_crc_t structure where the CRC values will be stored
 *
 * This function reads the CRC values from the Unigraf device and stores them in the
 * provided igt_crc_t structure.
 */
void unigraf_read_crc(int stream, igt_crc_t *out)
{
	unigraf_select_stream(stream);
	unigraf_assert(TSIX_TS_GetConfigItem(unigraf_device, TSI_DPRX_CRC_R_R,
					     &out->crc[0], sizeof(out->crc[0])));
	unigraf_assert(TSIX_TS_GetConfigItem(unigraf_device, TSI_DPRX_CRC_G_R,
					     &out->crc[1], sizeof(out->crc[1])));
	unigraf_assert(TSIX_TS_GetConfigItem(unigraf_device, TSI_DPRX_CRC_B_R,
					     &out->crc[2], sizeof(out->crc[2])));
	out->n_words = 3;
	out->has_valid_frame = false;
}

/**
 * unigraf_use_crc() - Check if Unigraf device should be used for CRC computation
 *
 * Returns: true if the Unigraf device should be used for CRC computation,
 *          false otherwise.
 */
bool unigraf_use_crc(void)
{
	return unigraf_crc;
}

static void unigraf_read_msa(void)
{
	uint32_t data = 1;

	unigraf_write_u32(TSI_DPRX_MSA_COMMAND_W, data);
}

/**
 * unigraf_assert_stream_timings() - Assert that the received stream on unigraf
 * matches the mode_info
 * @stream: Stream id on the unigraf
 * @mode_info: Mode to compare with
 */
void unigraf_assert_stream_timings(int stream, drmModeModeInfoPtr mode_info)
{
	uint32_t stream_count;

	igt_assert(mode_info);

	unigraf_read_msa();
	stream_count = unigraf_read_u32(TSI_DPRX_MSA_STREAM_COUNT_R);
	igt_assert_lt(stream, stream_count);
	unigraf_write_u32(TSI_DPRX_MSA_STREAM_SELECT, stream);
	igt_assert_eq(mode_info->htotal, unigraf_read_u32(TSI_DPRX_MSA_HTOTAL_R));
	igt_assert_eq(mode_info->vtotal, unigraf_read_u32(TSI_DPRX_MSA_VTOTAL_R));
	igt_assert_eq(mode_info->hdisplay, unigraf_read_u32(TSI_DPRX_MSA_HACTIVE_R));
	igt_assert_eq(mode_info->vdisplay, unigraf_read_u32(TSI_DPRX_MSA_VACTIVE_R));
	igt_assert_eq(mode_info->hsync_end - mode_info->hsync_start,
		      unigraf_read_u32(TSI_DPRX_MSA_HSYNC_WIDTH_R));
	igt_assert_eq(mode_info->vsync_end - mode_info->vsync_start,
		      unigraf_read_u32(TSI_DPRX_MSA_VSYNC_WIDTH_R));
	igt_assert_eq(mode_info->vsync_start, unigraf_read_u32(TSI_DPRX_MSA_VSTART_R));
	igt_assert_eq(mode_info->hsync_start, unigraf_read_u32(TSI_DPRX_MSA_HSTART_R));
}

/**
 * unigraf_set_max_lane_count() - Set the maximum number of lanes advertised to the DUT
 * @count: The maximum number of lanes to configure on the device
 *
 * This function sets the maximum number of lanes that the device will advertise on the DP link.
 * The actual number of lanes used may be less than the requested count if the
 * DUT does not support/use it.
 */
void unigraf_set_max_lane_count(uint32_t count)
{
	unigraf_write_u32(TSI_DPRX_MAX_LANES, count);
}

/**
 * unigraf_get_max_lane_count() - Get the maximum number of lanes supported by the device
 *
 * Returns: The maximum number of lanes supported by the device.
 */
int unigraf_get_max_lane_count(void)
{
	int max_lanes;

	igt_assert(unigraf_device);
	unigraf_assert(TSIX_TS_GetConfigItem(unigraf_device, TSI_DPRX_MAX_LANES,
					     &max_lanes, sizeof(max_lanes)));
	return max_lanes;
}

/**
 * unigraf_get_lt_rate() - Get the current link training rate
 *
 * Returns: The current link training rate in units of 270 MHz.
 */
uint32_t unigraf_get_lt_rate(void)
{
	return unigraf_read_u32(TSI_DPRX_LT_RATE_R);
}

/**
 * unigraf_get_lt_lane_count() - Get the current link training lane count
 *
 * Returns: The current number of lanes being used in link training.
 */
uint32_t unigraf_get_lt_lane_count(void)
{
	return unigraf_read_u32(TSI_DPRX_LT_LANE_COUNT_R);
}

/**
 * unigraf_set_max_link_rate() - Set the maximum link rate advertised to the DUT
 * @bandwidth: The maximum link rate to configure on the device. Actual value is
 *        @bandwidth * 270000 kHz. Common values can be found in enum unigraf_rate
 *
 * This function sets the maximum link rate that the device will advertise on the DP link.
 * The actual link rate used may be less than the requested value if the DUT does not
 * support/use it.
 */
void unigraf_set_max_link_rate(int bandwidth)
{
	unigraf_write_u32(TSI_DPRX_MAX_LINK_RATE, bandwidth);
}

/**
 * unigraf_get_max_link_rate() - Get the maximum link rate supported by the device
 *
 * Returns: The maximum link rate supported by the device. The value returned must be multiplied by
 *          270000 to get the value in kHz.
 */
int unigraf_get_max_link_rate(void)
{
	return unigraf_read_u32(TSI_DPRX_MAX_LINK_RATE);
}

/**
 * unigraf_rate_to_kbs - Convert a rate value to kilobits per second
 * @rate: The rate value to convert
 *
 * This function converts a rate value to kilobits per second (kbps).
 *
 * Returns: The converted rate in kilobits per second
 */
int unigraf_rate_to_kbs(enum unigraf_rate rate)
{
	switch (rate) {
	case UNIGRAF_RATE_1_62_GHZ:
		return 1620000;
	case UNIGRAF_RATE_2_7_GHZ:
		return 2700000;
	case UNIGRAF_RATE_5_4_GHZ:
		return 5400000;
	case UNIGRAF_RATE_6_75_GHZ:
		return 6750000;
	case UNIGRAF_RATE_8_10_GHZ:
		return 8100000;
	default:
		igt_assert_f(0, "Unknown rate %d\n", rate);
		return 0;
	}
}
