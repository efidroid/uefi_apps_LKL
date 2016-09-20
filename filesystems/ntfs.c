/*
 * Copyright (C) 2004 Kay Sievers <kay.sievers@vrfy.org>
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include "LKL.h"

CONST FS_IDINFO IdInfoNTFS =
{
	.Name		= "ntfs",
	.Magics		=
	{
		{ .magic = "NTFS    ", .len = 8, .sboff = 3 },
		{ NULL }
	}
};

