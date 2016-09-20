/*
 * Copyright (C) 2013 Alejandro Martinez Ruiz <alex@nowcomputing.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License
 */

#include "LKL.h"

#define F2FS_MAGIC		"\x10\x20\xF5\xF2"
#define F2FS_MAGIC_OFF		0
#define F2FS_SB1_OFF		0x400
#define F2FS_SB1_KBOFF		(F2FS_SB1_OFF >> 10)

CONST FS_IDINFO IdInfoF2FS =
{
	.Name           = "f2fs",
	.Magics         =
        {
		{
			.magic = F2FS_MAGIC,
			.len = 4,
			.kboff = F2FS_SB1_KBOFF,
			.sboff = F2FS_MAGIC_OFF
		},
		{ NULL }
	}
};
