/*
 * Copyright (C) 1999, 2001 by Andries Brouwer
 * Copyright (C) 1999, 2000, 2003 by Theodore Ts'o
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include "LKL.h"

/* magic string */
#define EXT_SB_MAGIC				"\123\357"
/* supper block offset */
#define EXT_SB_OFF				0x400
/* supper block offset in kB */
#define EXT_SB_KBOFF				(EXT_SB_OFF >> 10)
/* magic string offset within super block */
#define EXT_MAG_OFF				0x38

#define BLKID_EXT_MAGICS \
	{ \
		{	 \
			.magic = EXT_SB_MAGIC, \
			.len = sizeof(EXT_SB_MAGIC) - 1, \
			.kboff = EXT_SB_KBOFF, \
			.sboff = EXT_MAG_OFF \
		}, \
		{ NULL } \
	}

CONST FS_IDINFO IdInfoExt = {
  .Name   = "ext4",
  .Magics = BLKID_EXT_MAGICS,
};
