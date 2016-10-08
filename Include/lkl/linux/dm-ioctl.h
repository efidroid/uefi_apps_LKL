/*
 * Copyright (C) 2001 - 2003 Sistina Software (UK) Limited.
 * Copyright (C) 2004 - 2009 Red Hat, Inc. All rights reserved.
 *
 * This file is released under the LGPL.
 */

#ifndef _LKL_LINUX_DM_IOCTL_V4_H
#define _LKL_LINUX_DM_IOCTL_V4_H

#include <lkl/linux/types.h>

#define LKL_DM_DIR "mapper"		/* Slashes not supported */
#define LKL_DM_CONTROL_NODE "control"
#define LKL_DM_MAX_TYPE_NAME 16
#define LKL_DM_NAME_LEN 128
#define LKL_DM_UUID_LEN 129

/*
 * A traditional ioctl interface for the device mapper.
 *
 * Each device can have two tables associated with it, an
 * 'active' table which is the one currently used by io passing
 * through the device, and an 'inactive' one which is a table
 * that is being prepared as a replacement for the 'active' one.
 *
 * LKL_DM_VERSION:
 * Just get the version information for the ioctl interface.
 *
 * LKL_DM_REMOVE_ALL:
 * Remove all dm devices, destroy all tables.  Only really used
 * for debug.
 *
 * LKL_DM_LIST_DEVICES:
 * Get a list of all the dm device names.
 *
 * LKL_DM_DEV_CREATE:
 * Create a new device, neither the 'active' or 'inactive' table
 * slots will be filled.  The device will be in suspended state
 * after creation, however any io to the device will get errored
 * since it will be out-of-bounds.
 *
 * LKL_DM_DEV_REMOVE:
 * Remove a device, destroy any tables.
 *
 * LKL_DM_DEV_RENAME:
 * Rename a device or set its uuid if none was previously supplied.
 *
 * DM_SUSPEND:
 * This performs both suspend and resume, depending which flag is
 * passed in.
 * Suspend: This command will not return until all pending io to
 * the device has completed.  Further io will be deferred until
 * the device is resumed.
 * Resume: It is no longer an error to issue this command on an
 * unsuspended device.  If a table is present in the 'inactive'
 * slot, it will be moved to the active slot, then the old table
 * from the active slot will be _destroyed_.  Finally the device
 * is resumed.
 *
 * LKL_DM_DEV_STATUS:
 * Retrieves the status for the table in the 'active' slot.
 *
 * LKL_DM_DEV_WAIT:
 * Wait for a significant event to occur to the device.  This
 * could either be caused by an event triggered by one of the
 * targets of the table in the 'active' slot, or a table change.
 *
 * LKL_DM_TABLE_LOAD:
 * Load a table into the 'inactive' slot for the device.  The
 * device does _not_ need to be suspended prior to this command.
 *
 * LKL_DM_TABLE_CLEAR:
 * Destroy any table in the 'inactive' slot (ie. abort).
 *
 * LKL_DM_TABLE_DEPS:
 * Return a set of device dependencies for the 'active' table.
 *
 * LKL_DM_TABLE_STATUS:
 * Return the targets status for the 'active' table.
 *
 * LKL_DM_TARGET_MSG:
 * Pass a message string to the target at a specific offset of a device.
 *
 * LKL_DM_DEV_SET_GEOMETRY:
 * Set the geometry of a device by passing in a string in this format:
 *
 * "cylinders heads sectors_per_track start_sector"
 *
 * Beware that CHS geometry is nearly obsolete and only provided
 * for compatibility with dm devices that can be booted by a PC
 * BIOS.  See struct hd_geometry for range limits.  Also note that
 * the geometry is erased if the device size changes.
 */

/*
 * All ioctl arguments consist of a single chunk of memory, with
 * this structure at the start.  If a uuid is specified any
 * lookup (eg. for a DM_INFO) will be done on that, *not* the
 * name.
 */
struct lkl_dm_ioctl {
	/*
	 * The version number is made up of three parts:
	 * major - no backward or forward compatibility,
	 * minor - only backwards compatible,
	 * patch - both backwards and forwards compatible.
	 *
	 * All clients of the ioctl interface should fill in the
	 * version number of the interface that they were
	 * compiled with.
	 *
	 * All recognised ioctl commands (ie. those that don't
	 * return -LKL_ENOTTY) fill out this field, even if the
	 * command failed.
	 */
	__lkl__u32 version[3];	/* in/out */
	__lkl__u32 data_size;	/* total size of data passed in
				 * including this struct */

	__lkl__u32 data_start;	/* offset to start of data
				 * relative to start of this struct */

	__lkl__u32 target_count;	/* in/out */
	__lkl__s32 open_count;	/* out */
	__lkl__u32 flags;		/* in/out */

	/*
	 * event_nr holds either the event number (input and output) or the
	 * udev cookie value (input only).
	 * The LKL_DM_DEV_WAIT ioctl takes an event number as input.
	 * The DM_SUSPEND, LKL_DM_DEV_REMOVE and LKL_DM_DEV_RENAME ioctls
	 * use the field as a cookie to return in the DM_COOKIE
	 * variable with the uevents they issue.
	 * For output, the ioctls return the event number, not the cookie.
	 */
	__lkl__u32 event_nr;      	/* in/out */
	__lkl__u32 padding;

	__lkl__u64 dev;		/* in/out */

	char name[LKL_DM_NAME_LEN];	/* device name */
	char uuid[LKL_DM_UUID_LEN];	/* unique identifier for
				 * the block device */
	char data[7];		/* padding or data */
};

/*
 * Used to specify tables.  These structures appear after the
 * dm_ioctl.
 */
struct lkl_dm_target_spec {
	__lkl__u64 sector_start;
	__lkl__u64 length;
	__lkl__s32 status;		/* used when reading from kernel only */

	/*
	 * Location of the next dm_target_spec.
	 * - When specifying targets on a LKL_DM_TABLE_LOAD command, this value is
	 *   the number of bytes from the start of the "current" dm_target_spec
	 *   to the start of the "next" dm_target_spec.
	 * - When retrieving targets on a LKL_DM_TABLE_STATUS command, this value
	 *   is the number of bytes from the start of the first dm_target_spec
	 *   (that follows the dm_ioctl struct) to the start of the "next"
	 *   dm_target_spec.
	 */
	__lkl__u32 next;

	char target_type[LKL_DM_MAX_TYPE_NAME];

	/*
	 * Parameter string starts immediately after this object.
	 * Be careful to add padding after string to ensure correct
	 * alignment of subsequent dm_target_spec.
	 */
};

/*
 * Used to retrieve the target dependencies.
 */
struct lkl_dm_target_deps {
	__lkl__u32 count;	/* Array size */
	__lkl__u32 padding;	/* unused */
	__lkl__u64 dev[0];	/* out */
};

/*
 * Used to get a list of all dm devices.
 */
struct lkl_dm_name_list {
	__lkl__u64 dev;
	__lkl__u32 next;		/* offset to the next record from
				   the _start_ of this */
	char name[0];
};

/*
 * Used to retrieve the target versions
 */
struct lkl_dm_target_versions {
        __lkl__u32 next;
        __lkl__u32 version[3];

        char name[0];
};

/*
 * Used to pass message to a target
 */
struct lkl_dm_target_msg {
	__lkl__u64 sector;	/* Device sector */

	char message[0];
};

/*
 * If you change this make sure you make the corresponding change
 * to dm-ioctl.c:lookup_ioctl()
 */
enum {
	/* Top level cmds */
	LKL_DM_VERSION_CMD = 0,
	LKL_DM_REMOVE_ALL_CMD,
	LKL_DM_LIST_DEVICES_CMD,

	/* device level cmds */
	LKL_DM_DEV_CREATE_CMD,
	LKL_DM_DEV_REMOVE_CMD,
	LKL_DM_DEV_RENAME_CMD,
	LKL_DM_DEV_SUSPEND_CMD,
	LKL_DM_DEV_STATUS_CMD,
	LKL_DM_DEV_WAIT_CMD,

	/* Table level cmds */
	LKL_DM_TABLE_LOAD_CMD,
	LKL_DM_TABLE_CLEAR_CMD,
	LKL_DM_TABLE_DEPS_CMD,
	LKL_DM_TABLE_STATUS_CMD,

	/* Added later */
	LKL_DM_LIST_VERSIONS_CMD,
	LKL_DM_TARGET_MSG_CMD,
	LKL_DM_DEV_SET_GEOMETRY_CMD
};

#define LKL_DM_IOCTL 0xfd

#define LKL_DM_VERSION       _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_VERSION_CMD, struct lkl_dm_ioctl)
#define LKL_DM_REMOVE_ALL    _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_REMOVE_ALL_CMD, struct lkl_dm_ioctl)
#define LKL_DM_LIST_DEVICES  _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_LIST_DEVICES_CMD, struct lkl_dm_ioctl)

#define LKL_DM_DEV_CREATE    _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_DEV_CREATE_CMD, struct lkl_dm_ioctl)
#define LKL_DM_DEV_REMOVE    _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_DEV_REMOVE_CMD, struct lkl_dm_ioctl)
#define LKL_DM_DEV_RENAME    _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_DEV_RENAME_CMD, struct lkl_dm_ioctl)
#define LKL_DM_DEV_SUSPEND   _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_DEV_SUSPEND_CMD, struct lkl_dm_ioctl)
#define LKL_DM_DEV_STATUS    _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_DEV_STATUS_CMD, struct lkl_dm_ioctl)
#define LKL_DM_DEV_WAIT      _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_DEV_WAIT_CMD, struct lkl_dm_ioctl)

#define LKL_DM_TABLE_LOAD    _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_TABLE_LOAD_CMD, struct lkl_dm_ioctl)
#define LKL_DM_TABLE_CLEAR   _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_TABLE_CLEAR_CMD, struct lkl_dm_ioctl)
#define LKL_DM_TABLE_DEPS    _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_TABLE_DEPS_CMD, struct lkl_dm_ioctl)
#define LKL_DM_TABLE_STATUS  _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_TABLE_STATUS_CMD, struct lkl_dm_ioctl)

#define LKL_DM_LIST_VERSIONS _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_LIST_VERSIONS_CMD, struct lkl_dm_ioctl)

#define LKL_DM_TARGET_MSG	 _LKL_IOWR(LKL_DM_IOCTL, LKL_DM_TARGET_MSG_CMD, struct lkl_dm_ioctl)
#define LKL_DM_DEV_SET_GEOMETRY	_LKL_IOWR(LKL_DM_IOCTL, LKL_DM_DEV_SET_GEOMETRY_CMD, struct lkl_dm_ioctl)

#define LKL_DM_VERSION_MAJOR	4
#define LKL_DM_VERSION_MINOR	35
#define LKL_DM_VERSION_PATCHLEVEL	0
#define LKL_DM_VERSION_EXTRA	"-ioctl (2016-06-23)"

/* Status bits */
#define LKL_DM_READONLY_FLAG	(1 << 0) /* In/Out */
#define LKL_DM_SUSPEND_FLAG		(1 << 1) /* In/Out */
#define LKL_DM_PERSISTENT_DEV_FLAG	(1 << 3) /* In */

/*
 * Flag passed into ioctl STATUS command to get table information
 * rather than current status.
 */
#define LKL_DM_STATUS_TABLE_FLAG	(1 << 4) /* In */

/*
 * Flags that indicate whether a table is present in either of
 * the two table slots that a device has.
 */
#define LKL_DM_ACTIVE_PRESENT_FLAG   (1 << 5) /* Out */
#define LKL_DM_INACTIVE_PRESENT_FLAG (1 << 6) /* Out */

/*
 * Indicates that the buffer passed in wasn't big enough for the
 * results.
 */
#define LKL_DM_BUFFER_FULL_FLAG	(1 << 8) /* Out */

/*
 * This flag is now ignored.
 */
#define LKL_DM_SKIP_BDGET_FLAG	(1 << 9) /* In */

/*
 * Set this to avoid attempting to freeze any filesystem when suspending.
 */
#define LKL_DM_SKIP_LOCKFS_FLAG	(1 << 10) /* In */

/*
 * Set this to suspend without flushing queued ios.
 * Also disables flushing uncommitted changes in the thin target before
 * generating statistics for LKL_DM_TABLE_STATUS and LKL_DM_DEV_WAIT.
 */
#define LKL_DM_NOFLUSH_FLAG		(1 << 11) /* In */

/*
 * If set, any table information returned will relate to the inactive
 * table instead of the live one.  Always check LKL_DM_INACTIVE_PRESENT_FLAG
 * is set before using the data returned.
 */
#define LKL_DM_QUERY_INACTIVE_TABLE_FLAG	(1 << 12) /* In */

/*
 * If set, a uevent was generated for which the caller may need to wait.
 */
#define LKL_DM_UEVENT_GENERATED_FLAG	(1 << 13) /* Out */

/*
 * If set, rename changes the uuid not the name.  Only permitted
 * if no uuid was previously supplied: an existing uuid cannot be changed.
 */
#define LKL_DM_UUID_FLAG			(1 << 14) /* In */

/*
 * If set, all buffers are wiped after use. Use when sending
 * or requesting sensitive data such as an encryption key.
 */
#define LKL_DM_SECURE_DATA_FLAG		(1 << 15) /* In */

/*
 * If set, a message generated output data.
 */
#define LKL_DM_DATA_OUT_FLAG		(1 << 16) /* Out */

/*
 * If set with LKL_DM_DEV_REMOVE or LKL_DM_REMOVE_ALL this indicates that if
 * the device cannot be removed immediately because it is still in use
 * it should instead be scheduled for removal when it gets closed.
 *
 * On return from LKL_DM_DEV_REMOVE, LKL_DM_DEV_STATUS or other ioctls, this
 * flag indicates that the device is scheduled to be removed when it
 * gets closed.
 */
#define LKL_DM_DEFERRED_REMOVE		(1 << 17) /* In/Out */

/*
 * If set, the device is suspended internally.
 */
#define LKL_DM_INTERNAL_SUSPEND_FLAG	(1 << 18) /* Out */

#endif				/* _LINUX_DM_IOCTL_H */
