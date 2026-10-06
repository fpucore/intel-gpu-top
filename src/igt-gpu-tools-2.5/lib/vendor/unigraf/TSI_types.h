/* SPDX-License-Identifier: MIT */

// Current unigraf public release are not c-compatible, this file hardcode some values.
// The next release of libTSI should include a c-compatible TSI_types.h file, that will
// be directly used in place of this file.

#ifndef TSI_REG_H

#include <stdbool.h>
#include <stdint.h>

#define TSI_VERSION_TEXT			0x80000001
#define TSI_DEVCAP_VIDEO_CAPTURE		0x00000001
#define TSI_SEARCHOPTIONS_SHOW_DEVICES_IN_USE	0x00000001

#define TSI_EDID_TE_INPUT			0x1100
#define TSI_EDID_SELECT_STREAM			0x1102

#define TSI_BASE_LEGACY_GENERIC(offset)		(0x210 + (offset))
#define	TSI_FORCE_HOT_PLUG_STATE_W		TSI_BASE_LEGACY_GENERIC(0x2)

#define TSI_BASE_LEGACY_DPRX_MSA(offset)		(0x260 + (offset))
#define TSI_DPRX_MSA_COMMAND_W				TSI_BASE_LEGACY_DPRX_MSA(0x0)
#define TSI_DPRX_MSA_STREAM_COUNT_R			TSI_BASE_LEGACY_DPRX_MSA(0x1)
#define TSI_DPRX_MSA_STREAM_SELECT			TSI_BASE_LEGACY_DPRX_MSA(0x3)
#define TSI_DPRX_MSA_HTOTAL_R				TSI_BASE_LEGACY_DPRX_MSA(0x6)
#define TSI_DPRX_MSA_VTOTAL_R				TSI_BASE_LEGACY_DPRX_MSA(0x7)
#define TSI_DPRX_MSA_HACTIVE_R				TSI_BASE_LEGACY_DPRX_MSA(0x8)
#define TSI_DPRX_MSA_VACTIVE_R				TSI_BASE_LEGACY_DPRX_MSA(0x9)
#define TSI_DPRX_MSA_HSYNC_WIDTH_R			TSI_BASE_LEGACY_DPRX_MSA(0xa)
#define TSI_DPRX_MSA_VSYNC_WIDTH_R			TSI_BASE_LEGACY_DPRX_MSA(0xb)
#define TSI_DPRX_MSA_HSTART_R				TSI_BASE_LEGACY_DPRX_MSA(0xc)
#define TSI_DPRX_MSA_VSTART_R				TSI_BASE_LEGACY_DPRX_MSA(0xd)

#define TSI_DPRX_LINK_FLAGS_MST				0x01
#define TSI_DPRX_LINK_FLAGS_TPS3			0x02
#define TSI_DPRX_LINK_FLAGS_TPS4			0x03
#define TSI_DPRX_LINK_FLAGS_EDP				0x04
#define TSI_DPRX_NOT_DOCUMENTED_DP_128_132_SUPPORTED	0x10
#define TSI_DPRX_NOT_DOCUMENTED_SIDEBAND_MSG_SUPPORT	0x20

#define TSI_BASE_DPRX(offset)			(0x50000000u + 0x21000 + (offset))
#define TSI_DPRX_HW_CAPS_R			TSI_BASE_DPRX(0x4)

/**
 * struct TSI_DPRX_HW_CAPS_R_s - Structure representing the hardware capabilities of the DP RX.
 *
 * This structure defines the bitfields and fields that describe the hardware
 * capabilities of the DP RX (DisplayPort Receiver) interface. Each field
 * corresponds to a specific capability or feature supported by the hardware.
 *
 * This structure is used to interpret the value read from the
 * TSI_DPRX_HW_CAPS_R register.
 *
 * @mst: MST support
 * @hdcp_1_x: HDCP 1.x support.
 * @hdcp_2_x: HDCP 2.x support.
 * @fec_8_10_b: FEC for 8/10 link support.
 * @dsc_8_10_b: DSC for 8/10 link support.
 * @three_lanes: Three lane link configuration support.
 * @edp_link_rate: eDP link rates are supported.
 * @mst_stream_count: Number of MST streams supported.
 * @max_link_rate: Maximum link rate supported. The unit is not specified in documentation,
 *                 it is probably like other config rate = 0.27GHz * value
 * @force_link_config: Forced link configuration support.
 * @power_provision: Power provision support on DP_PWR pin of receptacle connector.
 * @aux_swing_voltage_control: AUX output voltage swing control support.
 * @custom_dp_rate: Custom DP 2.0 rate support.
 * @custom_bit_rate: Custom bit rate support.
 * @fec_128_132_b: FEC for 128/132 link support.
 * @dsc_128_132_b: DSC for 128/132 link support.
 */
struct TSI_DPRX_HW_CAPS_R_s {
	bool mst:1;
	bool hdcp_1_x:1;
	bool hdcp_2_x:1;
	bool fec_8_10_b:1;
	bool dsc_8_10_b:1;
	bool reserved_1:1;
	bool three_lanes:1;
	bool edp_link_rates_supported:1;
	uint8_t mst_stream_count:3;
	uint8_t reserved_2:5;
	uint8_t max_link_rate;
	bool force_link_config:1;
	bool reserved_3:1;
	bool power_provision:1;
	bool aux_swing_voltage_control:1;
	bool custom_dp_rate:1;
	bool custom_bit_rate:1;
	bool fec_128_132_b:1;
	bool dsc_128_132_b:1;
};

#define TSI_DPRX_LT_LANE_COUNT_R		TSI_BASE_DPRX(0x0B)
#define TSI_DPRX_LT_RATE_R			TSI_BASE_DPRX(0x0C)
#define TSI_DPRX_HPD_FORCE			TSI_BASE_DPRX(0x12)
#define TSI_DPRX_MST_SINK_COUNT			TSI_BASE_DPRX(0x9D)

#define TSI_BASE_DP_RX(offset)                  (0x00010100 + (offset))
#define TSI_DP_RX_DUT_MAX_LANE_COUNT		TSI_BASE_DP_RX(0xf)

#define TSI_BASE_DP_LTT(offset)                 (0x00010700 + (offset))
#define TSI_DP_LTT_MAX_LANE_COUNT		TSI_BASE_DP_LTT(0x01)

#define TSI_BASE_LEGACY_DPRX(offset)		(0x2b0 + (offset))
#define TSI_DPRX_DPCD_BASE_W			TSI_BASE_LEGACY_DPRX(0x9)
#define TSI_DPRX_DPCD_DATA			TSI_BASE_LEGACY_DPRX(0xA)
#define TSI_DPRX_MAX_LANES			TSI_BASE_LEGACY_DPRX(0x10)
#define TSI_DPRX_MAX_LINK_RATE			TSI_BASE_LEGACY_DPRX(0x11)
#define TSI_DPRX_LINK_FLAGS			TSI_BASE_LEGACY_DPRX(0x12)
#define TSI_DPRX_STREAM_SELECT			TSI_BASE_LEGACY_DPRX(0x13)
#define TSI_DPRX_CRC_R_R			TSI_BASE_LEGACY_DPRX(0x14)
#define TSI_DPRX_CRC_G_R			TSI_BASE_LEGACY_DPRX(0x15)
#define TSI_DPRX_CRC_B_R			TSI_BASE_LEGACY_DPRX(0x16)
#define TSI_DPRX_HPD_PULSE_W			TSI_BASE_LEGACY_DPRX(0x1B)

#define H2_SINK_LOAD_PROD_KEYS                         0x002
#define H2_SINK_UNLOAD_KEYS                            0x003
#define H2_SINK_SET_CAPABLE                            0x004
#define H2_SINK_CLEAR_CAPABLE                          0x005

#define TSI_BASE_LEGACY_HDCP2(offset)                  (0x290 + (offset))
#define TSI_HDCP_2X_COMMAND_W                          TSI_BASE_LEGACY_HDCP2(0x1)

#endif /* TSI_REG_H */
