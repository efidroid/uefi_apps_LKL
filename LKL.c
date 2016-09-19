/** @file
*
*  Copyright (c) 2011-2015, ARM Limited. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#include <Library/UefiApplicationEntryPoint.h>
#include <Library/BaseMemoryLib.h>
#include <PiDxe.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>

#include <Protocol/Cpu.h>

#include <lk/kernel/thread.h>

#include "LKL.h"

EFI_CPU_ARCH_PROTOCOL  *gCpu = NULL;
EFI_RESET_SYSTEM       gUefiResetSystem;

EFI_STATUS
EFIAPI
LKLDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

EFI_STATUS
EFIAPI
LKLDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

EFI_STATUS
EFIAPI
LKLDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Controller,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer
  );

//
// DriverBinding protocol instance
//
EFI_DRIVER_BINDING_PROTOCOL gLKLDriverBinding = {
  LKLDriverBindingSupported,
  LKLDriverBindingStart,
  LKLDriverBindingStop,
  0xa,
  NULL,
  NULL
};

EFI_STATUS
EFIAPI
LKLEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS                Status;
  long ret;

  // Get Cpu Arch protocol
  Status = gBS->LocateProtocol (&gEfiCpuArchProtocolGuid, NULL, (VOID **)&gCpu);
  ASSERT_EFI_ERROR(Status);

  lkl_thread_init();

  // start linux kernel
  ret = lkl_start_kernel(&lkl_host_ops, 256 * 1024 * 1024, "");
  if (ret) {
    DEBUG((EFI_D_ERROR, "can't start kernel: %s\n", lkl_strerror(ret)));
    return EFI_LOAD_ERROR;
  }

  //
  // Initialize the EFI Driver Library
  //
  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gLKLDriverBinding,
             ImageHandle,
             &gLKLComponentName,
             &gLKLComponentName2
             );
  ASSERT_EFI_ERROR (Status);

  return Status;
}

EFI_STATUS
EFIAPI
LKLUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *DeviceHandleBuffer;
  UINTN       DeviceHandleCount;
  UINTN       Index;
  VOID        *ComponentName;
  VOID        *ComponentName2;

  Status = gBS->LocateHandleBuffer (
                  AllHandles,
                  NULL,
                  NULL,
                  &DeviceHandleCount,
                  &DeviceHandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < DeviceHandleCount; Index++) {
    Status = EfiTestManagedDevice (DeviceHandleBuffer[Index], ImageHandle, &gEfiDiskIoProtocolGuid);
    if (!EFI_ERROR (Status)) {
      Status = gBS->DisconnectController (
                      DeviceHandleBuffer[Index],
                      ImageHandle,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        break;
      }
    }
  }

  if (Index == DeviceHandleCount) {
    //
    // Driver is stopped successfully.
    //
    Status = gBS->HandleProtocol (ImageHandle, &gEfiComponentNameProtocolGuid, &ComponentName);
    if (EFI_ERROR (Status)) {
      ComponentName = NULL;
    }

    Status = gBS->HandleProtocol (ImageHandle, &gEfiComponentName2ProtocolGuid, &ComponentName2);
    if (EFI_ERROR (Status)) {
      ComponentName2 = NULL;
    }

    if (ComponentName == NULL) {
      if (ComponentName2 == NULL) {
        Status = gBS->UninstallMultipleProtocolInterfaces (
                        ImageHandle,
                        &gEfiDriverBindingProtocolGuid,  &gLKLDriverBinding,
                        NULL
                        );
      } else {
        Status = gBS->UninstallMultipleProtocolInterfaces (
                        ImageHandle,
                        &gEfiDriverBindingProtocolGuid,  &gLKLDriverBinding,
                        &gEfiComponentName2ProtocolGuid, ComponentName2,
                        NULL
                        );
      }
    } else {
      if (ComponentName2 == NULL) {
        Status = gBS->UninstallMultipleProtocolInterfaces (
                        ImageHandle,
                        &gEfiDriverBindingProtocolGuid,  &gLKLDriverBinding,
                        &gEfiComponentNameProtocolGuid,  ComponentName,
                        NULL
                        );
      } else {
        Status = gBS->UninstallMultipleProtocolInterfaces (
                        ImageHandle,
                        &gEfiDriverBindingProtocolGuid,  &gLKLDriverBinding,
                        &gEfiComponentNameProtocolGuid,  ComponentName,
                        &gEfiComponentName2ProtocolGuid, ComponentName2,
                        NULL
                        );
      }
    }
  }

  if (DeviceHandleBuffer != NULL) {
    FreePool (DeviceHandleBuffer);
  }

  lkl_sys_halt();

  // this kills the kernel's main thread
  thread_yield();

  return Status;
}

EFI_STATUS
EFIAPI
LKLDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS            Status;
  EFI_DISK_IO_PROTOCOL  *DiskIo;

  //
  // Open the IO Abstraction(s) needed to perform the supported test
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **) &DiskIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );

  if (EFI_ERROR (Status)) {
    return Status;
  }
  //
  // Close the I/O Abstraction(s) used to perform the supported test
  //
  gBS->CloseProtocol (
         ControllerHandle,
         &gEfiDiskIoProtocolGuid,
         This->DriverBindingHandle,
         ControllerHandle
         );

  //
  // Open the IO Abstraction(s) needed to perform the supported test
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiBlockIoProtocolGuid,
                  NULL,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                  );

  return Status;
}

EFI_STATUS
EFIAPI
LKLDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS            Status;
  EFI_BLOCK_IO_PROTOCOL *BlockIo;
  EFI_DISK_IO_PROTOCOL  *DiskIo;
  EFI_DISK_IO2_PROTOCOL *DiskIo2;
  BOOLEAN               LockedByMe;

  LockedByMe = FALSE;
  //
  // Acquire the lock.
  // If caller has already acquired the lock, cannot lock it again.
  //
  Status = LKLAcquireLockOrFail ();
  if (!EFI_ERROR (Status)) {
    LockedByMe = TRUE;
  }

  Status = InitializeUnicodeCollationSupport (This->DriverBindingHandle);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }
  //
  // Open our required BlockIo and DiskIo
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **) &BlockIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **) &DiskIo,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDiskIo2ProtocolGuid,
                  (VOID **) &DiskIo2,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    DiskIo2 = NULL;
  }

  //
  // Allocate Volume structure. In LKLAllocateVolume(), Resources
  // are allocated with protocol installed and cached initialized
  //
  Status = LKLAllocateVolume (ControllerHandle, DiskIo, DiskIo2, BlockIo);

  //
  // When the media changes on a device it will Reinstall the BlockIo interaface.
  // This will cause a call to our Stop(), and a subsequent reentrant call to our
  // Start() successfully. We should leave the device open when this happen.
  //
  if (EFI_ERROR (Status)) {
    Status = gBS->OpenProtocol (
                    ControllerHandle,
                    &gEfiSimpleFileSystemProtocolGuid,
                    NULL,
                    This->DriverBindingHandle,
                    ControllerHandle,
                    EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      gBS->CloseProtocol (
             ControllerHandle,
             &gEfiDiskIoProtocolGuid,
             This->DriverBindingHandle,
             ControllerHandle
             );
      gBS->CloseProtocol (
             ControllerHandle,
             &gEfiDiskIo2ProtocolGuid,
             This->DriverBindingHandle,
             ControllerHandle
             );
    }
  }

Exit:
  //
  // Unlock if locked by myself.
  //
  if (LockedByMe) {
    LKLReleaseLock ();
  }
  return Status;
}

EFI_STATUS
EFIAPI
LKLDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL   *This,
  IN  EFI_HANDLE                    ControllerHandle,
  IN  UINTN                         NumberOfChildren,
  IN  EFI_HANDLE                    *ChildHandleBuffer
  )
{
  EFI_STATUS                      Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
  LKL_VOLUME                      *Volume;
  EFI_DISK_IO2_PROTOCOL           *DiskIo2;

  DiskIo2 = NULL;
  //
  // Get our context back
  //
  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  (VOID **) &FileSystem,
                  This->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );

  if (!EFI_ERROR (Status)) {
    Volume  = VOLUME_FROM_VOL_INTERFACE (FileSystem);
    DiskIo2 = Volume->DiskIo2;
    Status  = LKLAbandonVolume (Volume);
  }

  if (!EFI_ERROR (Status)) {
    if (DiskIo2 != NULL) {
      Status = gBS->CloseProtocol (
        ControllerHandle,
        &gEfiDiskIo2ProtocolGuid,
        This->DriverBindingHandle,
        ControllerHandle
        );
      ASSERT_EFI_ERROR (Status);
    }
    Status = gBS->CloseProtocol (
      ControllerHandle,
      &gEfiDiskIoProtocolGuid,
      This->DriverBindingHandle,
      ControllerHandle
      );
    ASSERT_EFI_ERROR (Status);
  }

  return Status;
}
