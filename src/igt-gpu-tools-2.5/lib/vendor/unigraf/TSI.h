/* SPDX-License-Identifier: MIT */

/*
 * tsi.h - Header for libTSI.so
 * Documentation here is taken from official documentation and developer observation.
 */

#ifndef TSI_H
#define TSI_H

#define TSI_CURRENT_VERSION	12
#define MAX_EDID_SIZE		4096

#define TSI_SUCCESS		0

typedef unsigned int		TSI_VERSION_ID;
typedef unsigned int		TSI_SEARCH_OPTIONS;
typedef unsigned int		TSI_DEVICE_CAPS;
typedef unsigned int		TSI_CONFIG_ID;
typedef unsigned int		TSI_DEVICE_ID;
typedef unsigned int		TSI_INPUT_ID;
typedef int			TSI_RESULT;
typedef void			*TSI_HANDLE;
typedef int			TSI_FLAGS;

/**
 * TSI_MISC_GetErrorDescription() - Get a human readable error message
 * @ErrorCode: Error code for which you want the message
 * @ErrorString: Pointer where to copy the message
 * @StringMaxLen: Size of the allocated string @ErrorString
 *
 * The official documentation states: If the function succeeds, the
 * return value is the number of characters required for the complete
 * error description string.
 * In reality, this function always returns 0 or < 0 (error), so there is no way to
 * tell if the allocated memory was big enough
 *
 * Returns:
 * - >= 0 on success, theorically the required string len to store the message
 * - < 0 on failure
 */
TSI_RESULT TSI_MISC_GetErrorDescription(TSI_RESULT ErrorCode,
					char *ErrorString,
					unsigned int StringMaxLen);

/**
 * TSI_Init() - Initialize the TSI library
 * @ClientVersion: Indicates the version used to call the libTSI.so functions.
 *
 * Initialize libTSI for use and sets up internal state. It can be called
 * multiple times, but TSI_Clean must be called the exact same number of time.
 *
 * Returns:
 * - In case of success: Reference count to the API (number of times to call TSI_Clean)
 * - TSI_ERROR_NOT_COMPATIBLE if the requested client version is not supported
 *   by the library
 * - TSI_ERROR_COMPATIBILITY_MISMATCH if TSI_Init is called twice with
 *   different client version
 */
TSI_RESULT TSI_Init(TSI_VERSION_ID ClientVersion);

/**
 * TSI_Clean() - Cleans and closes the TSI library
 *
 * When TSI_Clean is called for the last time, cleanup the internal state. It
 * should be called exactly the same number of times as TSI_Init
 */
TSI_RESULT TSI_Clean(void);

/**
 * TSIX_DEV_RescanDevices() - Refresh the internal list of devices for libTSI
 * @SearchOptions: Options to filter the list of devices (e.g.,
 *                 TSI_SEARCHOPTIONS_SHOW_DEVICES_IN_USE to include
 *                 devices already in use).
 * @RequiredCaps: Filter to list only devices with specific capabilities.
 * @UnallowedCaps: Filter to list only devices without specific capabilities.
 *
 * Returns: >=0 in case of success, <0 on error. The value is not the number of devices,
 *          you need to use TSIX_DEV_GetDeviceCount to get the discovered devices count.
 *
 * This function should be called every time you need to update the list of connected devices,
 * and it must be called at least once before calling TSI_DEV_GetDeviceCount.
 */
TSI_RESULT TSIX_DEV_RescanDevices(TSI_SEARCH_OPTIONS SearchOptions,
				  TSI_DEVICE_CAPS RequiredCaps,
				  TSI_DEVICE_CAPS UnallowedCaps);

/**
 * TSIX_DEV_GetDeviceCount() - Get the count of scanned devices
 * Returns: the number of devices that the previous call to TSIX_DEV_RescanDevices() detected
 *
 * Must be called after a TSIX_DEV_RescanDevices.
 */
TSI_RESULT TSIX_DEV_GetDeviceCount(void);

/**
 * TSIX_DEV_GetDeviceName() - Get the name of a device from the scanned list
 * @DeviceID: Index in the TSI_DEV_RescanDevices list
 * @DevNameString: Pointer to store the device name
 * @NameStringMaxLength: Size of the allocated memory for @DevNameString
 *
 * Returns: >=0 in case of success, the length of the device name string
 * Note: If the return value is larger than NameStringMaxLength, the string may be truncated
 */
TSI_RESULT TSIX_DEV_GetDeviceName(TSI_DEVICE_ID DeviceID, char *DevNameString,
				  unsigned int NameStringMaxLength);

/**
 * TSIX_DEV_OpenDevice() - Open a device from the scanned list
 * @DeviceID: index in the TSI_DEV_RescanDevices list
 * @Result: Pointer to store the error code returned while opening the device
 * Returns: if the device is found, an opaque pointer that can be used for other
 * API calls, or NULL on error.
 */
TSI_HANDLE TSIX_DEV_OpenDevice(TSI_DEVICE_ID DeviceID, TSI_RESULT *Result);

/**
 * TSIX_DEV_CloseDevice() - Close the device handle when finished
 * @Device: Device handle to close
 * Returns: >=0 in case of success
 */
TSI_RESULT TSIX_DEV_CloseDevice(TSI_HANDLE Device);

/**
 * TSIX_DEV_GetDeviceRoleCount() - Get the number of roles available for a device
 * @Device: Device handle to query
 *
 * Returns: >=0 in case of success, the number of roles available for the device
 */
TSI_RESULT TSIX_DEV_GetDeviceRoleCount(TSI_HANDLE Device);

/**
 * TSIX_DEV_GetDeviceRoleName() - Get the name of a specific role for a device
 * @Device: Device handle to query
 * @RoleIndex: Index of the role to get the name for
 * @RoleNameString: Pointer to store the role name
 * @RoleStringMaxLength: Size of the allocated memory for @RoleNameString
 *
 * Returns: >=0 in case of success, the length of the role name string
 * Note: If the return value is larger than RoleStringMaxLength, the string may be truncated
 */
TSI_RESULT TSIX_DEV_GetDeviceRoleName(TSI_HANDLE Device, int RoleIndex,
				      char *RoleNameString, unsigned int RoleStringMaxLength);

/**
 * TSIX_DEV_SelectRole() - Select a specific role for the specified device
 * @Device: Device handle on which to assign the role
 * @RoleIndex: Index of the role to assign
 * Returns: >=0 in case of success
 */
TSI_RESULT TSIX_DEV_SelectRole(TSI_HANDLE Device, int RoleIndex);

/**
 * TSIX_VIN_GetInputCount() - Get the number of video inputs available on a device
 * @Device: Device handle to query
 *
 * Returns: >=0 in case of success, the number of video inputs available on the device
 */
TSI_RESULT TSIX_VIN_GetInputCount(TSI_HANDLE Device);

/**
 * TSIX_VIN_GetInputName() - Get the name of a specific video input on a device
 * @Device: Device handle to query
 * @InputID: Identifier of the input to get the name for
 * @InputNameString: Pointer to store the input name
 * @NameStringMaxLen: Size of the allocated memory for @InputNameString
 *
 * Returns: >=0 in case of success, the length of the input name string
 * Note: If the return value is larger than NameStringMaxLen, the string may be truncated
 */
TSI_RESULT TSIX_VIN_GetInputName(TSI_HANDLE Device, TSI_INPUT_ID InputID,
				 char *InputNameString, unsigned int NameStringMaxLen);

/**
 * TSIX_VIN_Disable() - Disable video input on the specified device
 * @Device: Device handle to disable video input on
 * Returns: >=0 in case of success
 */
TSI_RESULT TSIX_VIN_Disable(TSI_HANDLE Device);

/**
 * TSIX_VIN_Select() - Select a specific video input on the specified device
 * @Device: Device handle on which to select the input
 * @InputID: Identifier of the input to select
 * Returns: >=0 in case of success
 */

TSI_RESULT TSIX_VIN_Select(TSI_HANDLE Device, TSI_INPUT_ID InputID);
/**
 * TSIX_VIN_Enable() - Enable video input on the specified device
 * @Device: Device handle to enable video input on
 * @Flags: Flags to specify the options for enabling the video input
 * Returns: >=0 in case of success
 */
TSI_RESULT TSIX_VIN_Enable(TSI_HANDLE Device, TSI_FLAGS Flags);

/**
 * TSIX_TS_GetConfigItem() - Read a configuration item from the UCD device
 * @Device: Device handle to read config from. Can be NULL for certain configuration items,
 *          for example TSI_VERSION_TEXT.
 * @ConfigItemID: Identifier of the requested configuration item.
 * @ConfigItemData: Pointer to store the read value.
 * @ItemMaxSize: Size of the allocated memory for @ConfigItemData.
 *
 * Returns: The size of the raw data. If the return value is larger than ItemMaxSize, no data
 *          is copied to ConfigItemData.
 * Note:
 * - Some configurations require specific size and alignment for the allocated buffer. Refer to
 *   the specific item configuration documentation for details.
 * - Data may still be written to @ConfigItemData even if the return value is larger than
 *   @ItemMaxSize, potentially causing buffer overflow.
 */
TSI_RESULT TSIX_TS_GetConfigItem(TSI_HANDLE Device, TSI_CONFIG_ID ConfigItemID,
				 void *ConfigItemData,
				 unsigned int ItemMaxSize);

/**
 * TSIX_TS_SetConfigItem() - Set a configuration item on the specified device
 * @Device: Device handle to set the configuration on
 * @ConfigItemID: Identifier of the configuration item to set
 * @ItemData: Pointer to the data to set
 * @ItemSize: Size of the data to set
 * Returns: >=0 in case of success
 */
TSI_RESULT TSIX_TS_SetConfigItem(TSI_HANDLE Device, TSI_CONFIG_ID ConfigItemID,
				 const void *ItemData, unsigned int ItemSize);

#endif
