/*++

Copyright (c) 2005 - 2013, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2016, The EFIDroid Project. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available
under the terms and conditions of the BSD License which accompanies this
distribution. The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.


Module Name:

  flush.c

Abstract:

  Routines that check references and flush OFiles

Revision History

--*/

#include "LKL.h"

EFI_STATUS
EFIAPI
LKLFlush (
  IN EFI_FILE_PROTOCOL  *FHand
  )
{
  LKL_IFILE  *IFile;
  LKL_VOLUME *Volume;
  EFI_STATUS Status;
  INTN       RC;

  IFile = IFILE_FROM_FHAND (FHand);
  Volume  = IFile->Volume;
  (VOID)(Volume);

  //
  // Lock the volume
  //
  LKLAcquireLock ();

  RC = lkl_sys_fsync(IFile->FD);
  if (RC) {
    Status = EFI_DEVICE_ERROR;
  }
  else {
    Status = EFI_SUCCESS;
  }

  //
  // Done. Unlock the volume
  //
  LKLReleaseLock ();

  return Status;
}

EFI_STATUS
EFIAPI
LKLFlushEx (
  IN EFI_FILE_PROTOCOL  *FHand,
  IN EFI_FILE_IO_TOKEN  *Token
  )
{
  DEBUG((EFI_D_ERROR, "%a:%u\n", __func__, __LINE__));
  return EFI_DEVICE_ERROR;
}

EFI_STATUS
EFIAPI
LKLClose (
  IN EFI_FILE_PROTOCOL  *FHand
  )
{
  LKL_IFILE  *IFile;
  LKL_VOLUME *Volume;

  IFile = IFILE_FROM_FHAND (FHand);
  Volume  = IFile->Volume;
  (VOID)(Volume);

  //
  // Lock the volume
  //
  LKLAcquireLock ();

  //
  // Close the file instance handle
  //
  LKLIFileClose (IFile);

  //
  // Done. Unlock the volume
  //
  LKLReleaseLock ();

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
LKLDelete (
  IN EFI_FILE_PROTOCOL  *FHand
  )
{
  INTN       RC;
  EFI_STATUS Status;
  LKL_IFILE  *IFile;
  LKL_VOLUME *Volume;

  IFile = IFILE_FROM_FHAND (FHand);
  Volume  = IFile->Volume;
  (VOID)(Volume);

  Status = EFI_WARN_DELETE_FAILURE;

  //
  // Lock the volume
  //
  LKLAcquireLock ();

  UINTN  FilePathSize = AsciiStrLen(Volume->LKLMountPoint) + 1 + AsciiStrLen(IFile->FilePath) + 1;
  CHAR8* FilePath = AllocatePool(FilePathSize);
  if (FilePath) {
    AsciiSPrint(FilePath, FilePathSize, "%a/%a", Volume->LKLMountPoint, IFile->FilePath);
  }

  //
  // Close the file instance handle
  //
  LKLIFileClose (IFile);

  if (FilePath) {
    //
    // Close the file instance handle
    //
    RC = lkl_sys_unlink(FilePath);
    if (RC==0) {
      Status = EFI_SUCCESS;
    }

    FreePool (FilePath);
  }

  //
  // Done. Unlock the volume
  //
  LKLReleaseLock ();

  return Status;
}
