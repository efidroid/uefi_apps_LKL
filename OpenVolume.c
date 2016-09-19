/*++

Copyright (c) 2005 - 2013, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available
under the terms and conditions of the BSD License which accompanies this
distribution. The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.


Module Name:

  OpenVolume.c

Abstract:

  OpenVolume() function of Simple File System Protocol

Revision History

--*/

#include "LKL.h"

EFI_STATUS
EFIAPI
LKLOpenVolume (
  IN  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *This,
  OUT EFI_FILE_PROTOCOL                **File
  )
{
  EFI_STATUS  Status;
  LKL_VOLUME  *Volume;
  LKL_IFILE   *IFile;
  INTN        FD;

  Volume = VOLUME_FROM_VOL_INTERFACE (This);
  LKLAcquireLock ();
  FD = -1;

  FD = lkl_sys_open(Volume->LKLMountPoint, LKL_O_RDONLY, 0644);
  if(FD<0) {
    Status = EFI_VOLUME_CORRUPTED;
    goto Done;
  }

  //
  // Open a new instance to the root
  //
  Status = LKLAllocateIFile (Volume, FD, &IFile);
  if (!EFI_ERROR (Status)) {
    *File = &IFile->Handle;
  }
  IFile->LinuxOpenMode = LKL_O_RDONLY;

Done:
  if (EFI_ERROR(Status)) {
    if (FD>=0)
      lkl_sys_close(FD);
  }

  LKLReleaseLock ();

  return Status;
}
