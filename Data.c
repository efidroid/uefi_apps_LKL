/*++

Copyright (c) 2005 - 2013, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available
under the terms and conditions of the BSD License which accompanies this
distribution. The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.


Module Name:

  Data.c

Abstract:

  Global data in the FAT Filesystem driver

Revision History

--*/

#include "LKL.h"

//
// Globals
//
//
// FatFsLock - Global lock for synchronizing all requests.
//
EFI_LOCK LKLFsLock   = EFI_INITIALIZE_LOCK_VARIABLE (TPL_CALLBACK);

//
// Filesystem interface functions
//
EFI_FILE_PROTOCOL               LKLFileInterface = {
  EFI_FILE_PROTOCOL_REVISION,
  LKLOpen,
  LKLClose,
  LKLDelete,
  LKLRead,
  LKLWrite,
  LKLGetPosition,
  LKLSetPosition,
  LKLGetInfo,
  LKLSetInfo,
  LKLFlush,
  LKLOpenEx,
  LKLReadEx,
  LKLWriteEx,
  LKLFlushEx
};
