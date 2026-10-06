// SPDX-License-Identifier: MIT
/*
 * A helper library for parsing and making use of real EDID data from monitors
 * and make them compatible with IGT and Chamelium.
 *
 * Copyright 2022 Google LLC.
 *
 * Authors: Mark Yacoub <markyacoub@chromium.org>
 */

#include "monitor_edids_helper.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "igt_core.h"
#include "igt_edid.h"
#include "dp_edids.h"
#include "drmtest.h"
#include "hdmi_edids.h"

struct {
	struct monitor_edid *edid_list;
	int list_size;
} ALL_EDIDS[] = {
	{DP_EDIDS_NON_4K,	DP_EDIDS_NON_4K_COUNT},
	{DP_EDIDS_4K,		DP_EDIDS_4K_COUNT},
	{HDMI_EDIDS_NON_4K,	HDMI_EDIDS_NON_4K_COUNT},
	{HDMI_EDIDS_4K,		HDMI_EDIDS_4K_COUNT},
};

static uint8_t convert_hex_char_to_byte(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;

	assert(0);
	return 0;
}

static uint8_t *get_edid_bytes_from_hex_str(const char *edid_str)
{
	int i;

	int edid_size = strlen(edid_str) / 2; /* each asci is a nibble. */
	uint8_t *edid = (uint8_t *)malloc(edid_size);

	for (i = 0; i < edid_size; i++) {
		edid[i] = convert_hex_char_to_byte(edid_str[i * 2]) << 4 |
			  convert_hex_char_to_byte(edid_str[i * 2 + 1]);
	}

	return edid;
}

const char *monitor_edid_get_name(const monitor_edid *edid)
{
	return edid->name;
}

struct chamelium_edid *
get_chameleon_edid_from_monitor_edid(struct chamelium *chamelium,
				     const monitor_edid *edid)
{
	int i;
	struct chamelium_edid *chamelium_edid;

	uint8_t *base_edid = get_edid_bytes_from_hex_str(edid->edid);
	assert(base_edid);

	/*Print the full formatted EDID on debug. */
	for (i = 0; i < strlen(edid->edid) / 2; i++) {
		igt_debug("%02x ", base_edid[i]);
		if (i % 16 == 15)
			igt_debug("\n");
	}

	chamelium_edid = malloc(sizeof(struct chamelium_edid));
	assert(chamelium_edid);

	chamelium_edid->base = (struct edid *)base_edid;
	chamelium_edid->chamelium = chamelium;
	for (i = 0; i < CHAMELIUM_MAX_PORTS; i++) {
		chamelium_edid->raw[i] = NULL;
		chamelium_edid->ids[i] = 0;
	}

	return chamelium_edid;
}

void free_chamelium_edid_from_monitor_edid(struct chamelium_edid *edid)
{
	int i;

	free(edid->base);
	for (i = 0; i < CHAMELIUM_MAX_PORTS; i++)
		free(edid->raw[i]);

	free(edid);
	edid = NULL;
}

/**
 * edid_from_monitor_edid:
 * @mon_edid: Monitor EDID to convert
 *
 * Get a struct edid from a monitor_edid. This returns a pointer to a newly allocated struct edid.
 * The caller is in charge to free this pointer when required.
 */
struct edid *edid_from_monitor_edid(const monitor_edid *mon_edid)
{
	uint8_t *raw_edid;
	size_t edid_size;
	int i;

	edid_size = strlen(mon_edid->edid) / 2; /* each ascii is a nibble. */
	raw_edid = malloc(edid_size);
	igt_assert(raw_edid);

	for (i = 0; i < edid_size; i++) {
		raw_edid[i] = convert_hex_char_to_byte(mon_edid->edid[i * 2]) << 4 |
			      convert_hex_char_to_byte(mon_edid->edid[i * 2 + 1]);
	}

	if (edid_get_size((struct edid *)raw_edid) > edid_size) {
		uint8_t *new_edid;

		igt_debug("The edid size stored in the raw edid is longer than the edid stored in the table.");
		new_edid = realloc(raw_edid, edid_get_size((struct edid *)raw_edid));
		igt_assert(new_edid);
		raw_edid = new_edid;
	}

	return (struct edid *)raw_edid;
}

/**
 * get_edids_for_connector_type:
 * @type: The connector type to get the EDIDs from
 * @count: Used to store the number of EDIDs in the returned list
 * @four_k: Use true to fetch 4k EDIDs, false to fetch non-4k EDIDs
 *
 * Get the list of EDIDS for a specific connector type. This returns a pointer to a static list,
 * so no need to free the pointer.
 */
const struct monitor_edid *get_edids_for_connector_type(uint32_t type, size_t *count, bool four_k)
{
	if (four_k) {
		switch (type) {
		case DRM_MODE_CONNECTOR_DisplayPort:
			*count = DP_EDIDS_4K_COUNT;
			return DP_EDIDS_4K;
		case DRM_MODE_CONNECTOR_HDMIA:
			*count = HDMI_EDIDS_4K_COUNT;
			return HDMI_EDIDS_4K;
		default:
			*count = 0;
			igt_debug("No 4k EDID for the connector %s\n",
				  kmstest_connector_type_str(type));
			return NULL;
		}
	} else {
		switch (type) {
		case DRM_MODE_CONNECTOR_DisplayPort:
			*count = DP_EDIDS_NON_4K_COUNT;
			return DP_EDIDS_NON_4K;
		case DRM_MODE_CONNECTOR_HDMIA:
			*count = HDMI_EDIDS_NON_4K_COUNT;
			return HDMI_EDIDS_NON_4K;
		default:
			*count = 0;
			igt_debug("No EDID for the connector %s\n",
				  kmstest_connector_type_str(type));
			return NULL;
		}
	}
}

/**
 * get_edid_by_name:
 * @name: Name to search in available EDIDs
 *
 * Return the struct edid associated with a specific name. As with edid_from_monitor_edid, the
 * caller must ensure to free the EDID after use. If no EDID with the exact name is found, returns
 * NULL.
 */
struct edid *get_edid_by_name(const char *name)
{
	for (int i = 0; i < ARRAY_SIZE(ALL_EDIDS); i++) {
		for (int j = 0; j < ALL_EDIDS[i].list_size; j++) {
			if (strncmp(ALL_EDIDS[i].edid_list[j].name, name, EDID_NAME_MAX_LEN) == 0)
				return edid_from_monitor_edid(&ALL_EDIDS[i].edid_list[j]);
		}
	}

	return NULL;
}

/*
 * list_edid_names:
 * @level: Log level to write the names on
 *
 * Print all the EDID available in igt.
 */
void list_edid_names(enum igt_log_level level)
{
	for (int i = 0; i < ARRAY_SIZE(ALL_EDIDS); i++) {
		for (int j = 0; j < ALL_EDIDS[i].list_size; j++) {
			igt_log(IGT_LOG_DOMAIN, level, " - \"%s\"\n",
				ALL_EDIDS[i].edid_list[j].name);
		}
	}
}
