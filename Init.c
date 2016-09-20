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

  Init.c

Abstract:

  Initialization routines

--*/

#include "LKL.h"
#include <stdio.h>

EFI_STATUS
LKLAllocateVolume (
  IN  EFI_HANDLE                Handle,
  IN  EFI_DISK_IO_PROTOCOL      *DiskIo,
  IN  EFI_DISK_IO2_PROTOCOL     *DiskIo2,
  IN  EFI_BLOCK_IO_PROTOCOL     *BlockIo
  )
{
  EFI_STATUS  Status;
  LKL_VOLUME  *Volume;
  INTN        Ret;
  CONST CHAR8 *FsType;

  Status = GetFsType (DiskIo, BlockIo->Media->MediaId, &FsType);
  if (EFI_ERROR(Status)) {
    return EFI_UNSUPPORTED;
  }

  //
  // Allocate a volume structure
  //
  Volume = AllocateZeroPool (sizeof (LKL_VOLUME));
  if (Volume == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Initialize the structure
  //
  Volume->Signature                   = LKL_VOLUME_SIGNATURE;
  Volume->Handle                      = Handle;
  Volume->DiskIo                      = DiskIo;
  Volume->DiskIo2                     = DiskIo2;
  Volume->BlockIo                     = BlockIo;
  Volume->MediaId                     = BlockIo->Media->MediaId;
  Volume->ReadOnly                    = BlockIo->Media->ReadOnly;
  Volume->VolumeInterface.Revision    = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION;
  Volume->LKLDisk.handle              = Volume;
  Volume->VolumeInterface.OpenVolume  = LKLOpenVolume;

  // register disk
  Ret = lkl_disk_add(&Volume->LKLDisk);
  if (Ret < 0) {
    DEBUG((EFI_D_ERROR, "can't add disk: %a\n", lkl_strerror(Ret)));
    Status = EFI_DEVICE_ERROR;
    goto Done;
  }
  Volume->LKLDiskId = Ret;

  // mount disk
  Ret = lkl_mount_dev(Volume->LKLDiskId, FsType, LKL_MS_SYNCHRONOUS|LKL_MS_DIRSYNC, NULL, Volume->LKLMountPoint, sizeof(Volume->LKLMountPoint));
  if (Ret < 0) {
    DEBUG((EFI_D_ERROR, "can't mount disk: %a\n", lkl_strerror(Ret)));
    Status = EFI_UNSUPPORTED;
    goto Done;
  }
  //
  // Install our protocol interfaces on the device's handle
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Volume->Handle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  &Volume->VolumeInterface,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    goto Done;
  }
  //
  // Volume installed
  //
  DEBUG ((EFI_D_INIT, "Installed LKL filesystem on %p\n", Handle));
  Volume->Valid = TRUE;

Done:
  if (EFI_ERROR (Status)) {
    lkl_disk_remove(Volume->LKLDisk);
    LKLFreeVolume (Volume);
  }

  return Status;
}

EFI_STATUS
LKLAbandonVolume (
  IN LKL_VOLUME *Volume
  )
{
  EFI_STATUS  Status;
  BOOLEAN     LockedByMe;

  //
  // Uninstall the protocol interface.
  //
  if (Volume->Handle != NULL) {
    Status = gBS->UninstallMultipleProtocolInterfaces (
                    Volume->Handle,
                    &gEfiSimpleFileSystemProtocolGuid,
                    &Volume->VolumeInterface,
                    NULL
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  LockedByMe = FALSE;

  //
  // Acquire the lock.
  // If the caller has already acquired the lock (which
  // means we are in the process of some Fat operation),
  // we can not acquire again.
  //
  Status = LKLAcquireLockOrFail ();
  if (!EFI_ERROR (Status)) {
    LockedByMe = TRUE;
  }

  Volume->Valid = FALSE;

  //
  // Release the lock.
  // If locked by me, this means DriverBindingStop is NOT
  // called within an on-going Fat operation, so we should
  // take responsibility to cleanup and free the volume.
  // Otherwise, the DriverBindingStop is called within an on-going
  // Fat operation, we shouldn't check reference, so just let outer
  // FatCleanupVolume do the task.
  //
  if (LockedByMe) {
    LKLReleaseLock ();
  }

  return EFI_SUCCESS;
}
