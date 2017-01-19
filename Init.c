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

STATIC
EFI_STATUS
LKLGetKeyLocation (
  IN  EFI_HANDLE                Handle,
  OUT CHAR16                    *OutPath,
  IN  UINTN                     OutPathSize
)
{
  EFI_DEVICE_PATH_PROTOCOL  *DevPath;
  EFI_GUID          *TmpGuid;
  UINT32            GuidData1;
  UINT16            GuidData2;
  UINT16            GuidData3;

  // get devpath
  DevPath = DevicePathFromHandle(Handle);
  if (DevPath == NULL)
    return EFI_INVALID_PARAMETER;

  // get HD devpath
  EFI_DEVICE_PATH_PROTOCOL *Node = DevPath;
  BOOLEAN Found = FALSE;
  while (!IsDevicePathEnd (Node)) {
    if (DevicePathType (Node) == MEDIA_DEVICE_PATH &&
        DevicePathSubType (Node) == MEDIA_HARDDRIVE_DP
        ) {
      Found = TRUE;
      break;
    }
    Node = NextDevicePathNode (Node);
  }
  if(Found == FALSE)
    return EFI_INVALID_PARAMETER;

  // build guid part
  HARDDRIVE_DEVICE_PATH *Hd = (HARDDRIVE_DEVICE_PATH*) Node;
  switch (Hd->SignatureType) {
  case SIGNATURE_TYPE_GUID:
    TmpGuid = (EFI_GUID *) &(Hd->Signature[0]);
    GuidData1 = ReadUnaligned32 (&(TmpGuid->Data1));
    GuidData2 = ReadUnaligned16 (&(TmpGuid->Data2));
    GuidData3 = ReadUnaligned16 (&(TmpGuid->Data3));

    UnicodeSPrint(OutPath, OutPathSize, L"\\misc\\vold\\expand_%08x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x.key",
      GuidData1,
      GuidData2,
      GuidData3,
      TmpGuid->Data4[0],
      TmpGuid->Data4[1],
      TmpGuid->Data4[2],
      TmpGuid->Data4[3],
      TmpGuid->Data4[4],
      TmpGuid->Data4[5],
      TmpGuid->Data4[6],
      TmpGuid->Data4[7]
      );

    UnicodeToLower (OutPath);
    break;

  default:
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
LKLAllocateVolume (
  IN  EFI_HANDLE                Handle,
  IN  EFI_DISK_IO_PROTOCOL      *DiskIo,
  IN  EFI_DISK_IO2_PROTOCOL     *DiskIo2,
  IN  EFI_BLOCK_IO_PROTOCOL     *BlockIo
  )
{
  EFI_STATUS  Status;
  LKL_VOLUME  *Volume = NULL;
  INTN        Ret;
  CONST CHAR8 *FsType;
  BOOLEAN     IsEncrypted = FALSE;
  CHAR16            KeyLocation[100];
  EFI_FILE_PROTOCOL *KeyFile = NULL;
  UINT64            KeyFileSize;
  UINT8             *Key = NULL;
  EFI_PARTITION_NAME_PROTOCOL *PartitionName;

  PartitionName = NULL;
  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiPartitionNameProtocolGuid,
                  (VOID **)&PartitionName
                  );
  if (!EFI_ERROR (Status)) {
    if (!StrCmp(PartitionName->Name, L"android_expand")) {
      IsEncrypted = TRUE;

      // build path for key file
      Status = LKLGetKeyLocation(Handle, KeyLocation, sizeof(KeyLocation));
      if (EFI_ERROR(Status)) {
        return Status;
      }

      // search existing partitions for key file
      Status = GetFileFromAnyPartition(KeyLocation, &KeyFile);
      if (EFI_ERROR(Status)) {
        return EFI_NOT_READY;
      }

      // get key file size
      Status = FileHandleGetSize(KeyFile, &KeyFileSize);
      if (EFI_ERROR(Status)) {
        return Status;
      }

      // allocate data for key
      Key = AllocatePool(KeyFileSize);
      if (Key == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      // read key
      UINTN BufferSize = KeyFileSize;
      Status = KeyFile->Read(KeyFile, &BufferSize, Key);
      if (EFI_ERROR(Status)) {
        goto Done;
      }
    }
  }

  if (IsEncrypted==FALSE) {
    Status = GetFsType (DiskIo, BlockIo->Media->MediaId, &FsType);
    if (EFI_ERROR(Status)) {
      return EFI_UNSUPPORTED;
    }
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
  Volume->FsType                      = FsType;
  Volume->IsEncrypted                 = IsEncrypted;

  // register disk
  Volume->LKLDiskId = -1;
  Ret = lkl_disk_add(&Volume->LKLDisk);
  if (Ret < 0) {
    DEBUG((EFI_D_ERROR, "can't add disk: %a\n", lkl_strerror(Ret)));
    Status = LKLError2EfiError(Ret);
    goto Done;
  }
  Volume->LKLDiskId = Ret;

  if (Volume->IsEncrypted) {
    UINT32 DevId;

    // get device id
    Ret = lkl_get_virtio_blkdev(Volume->LKLDiskId, 0, &DevId);
    if (Ret < 0) {
      Status = LKLError2EfiError(Ret);
      goto Done;
    }

    // build path to block device
    AsciiSPrint(Volume->LKLBlkDevice, sizeof(Volume->LKLBlkDevice), "/dev/%08x", DevId);

    // create node
    Ret = lkl_sys_mknod(Volume->LKLBlkDevice, LKL_S_IFBLK | 0600, DevId);
    if (Ret < 0) {
      Status = LKLError2EfiError(Ret);
      goto Done;
    }

    // build cryptfs name
    AsciiSPrint(Volume->LKLCryptFSName, sizeof(Volume->LKLCryptFSName), "lkl-%08x", DevId);

    // setup dmcrypt
    Ret = cryptfs_setup_ext_volume(Volume->LKLCryptFSName, Volume->LKLBlkDevice, Key, KeyFileSize, Volume->LKLBlkDeviceDecrypted);
    if (Ret < 0) {
      Status = LKLError2EfiError(Ret);
      goto Done;
    }

    // get fs type
    Status = GetFsTypeLKL (Volume->LKLBlkDeviceDecrypted, &FsType);
    if (EFI_ERROR(Status)) {
      goto Done;
    }

    // build path to mount point
    AsciiSPrint(Volume->LKLMountPoint, sizeof(Volume->LKLMountPoint), "/mnt/%08x", DevId);

    // create mount point directory
    Status = LKLMakeDir(Volume->LKLMountPoint);
    if (EFI_ERROR(Status)) {
      goto Done;
    }

    // mount disk
    Ret = lkl_sys_mount(Volume->LKLBlkDeviceDecrypted, Volume->LKLMountPoint, (CHAR8*)FsType, LKL_MS_SYNCHRONOUS|LKL_MS_DIRSYNC, NULL);
    if (Ret < 0) {
      DEBUG((EFI_D_ERROR, "can't mount disk: %a\n", lkl_strerror(Ret)));
      Status = LKLError2EfiError(Ret);
      goto Done;
    }
  }

  else {
    // mount disk
    Ret = lkl_mount_dev(Volume->LKLDiskId, 0, FsType, LKL_MS_SYNCHRONOUS|LKL_MS_DIRSYNC, NULL, Volume->LKLMountPoint, sizeof(Volume->LKLMountPoint));
    if (Ret < 0) {
      DEBUG((EFI_D_ERROR, "can't mount disk: %a\n", lkl_strerror(Ret)));
      Status = LKLError2EfiError(Ret);
      goto Done;
    }
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
  if (EFI_ERROR (Status) && Volume) {
    if (Volume->LKLMountPoint[0])
      lkl_sys_umount(Volume->LKLMountPoint, 0);

    if (Volume->IsEncrypted) {
      if (Volume->LKLCryptFSName[0])
        cryptfs_revert_ext_volume(Volume->LKLCryptFSName);

      if (Volume->LKLBlkDevice[0])
        lkl_sys_unlink(Volume->LKLBlkDevice);
    }

    if (Volume->LKLDiskId >= 0)
        lkl_disk_remove(Volume->LKLDisk);

    LKLFreeVolume (Volume);
  }

  if (Key) {
    FreePool (Key);
  }

  if (KeyFile) {
    FileHandleClose (KeyFile);
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
