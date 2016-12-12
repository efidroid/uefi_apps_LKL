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

  LKL.h

Abstract:

  Main header file for EFI FAT file system driver

Revision History

--*/

#ifndef _FAT_H_
#define _FAT_H_

#include <Uefi.h>

#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <Guid/FileSystemVolumeLabelInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>
#include <Protocol/DiskIo2.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/UnicodeCollation.h>
#include <Protocol/PartitionName.h>

#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/DevicePathLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/FileHandleLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <lkl.h>
#include <lkl_host.h>
#include <sys/param.h>

#include <FsId.h>

//
// The LKL signature
//
#define LKL_VOLUME_SIGNATURE         SIGNATURE_32 ('l', 'k', 'l', 'v')
#define LKL_IFILE_SIGNATURE          SIGNATURE_32 ('l', 'k', 'l', 'i')

#define VOLUME_FROM_VOL_INTERFACE(a) CR (a, LKL_VOLUME, VolumeInterface, LKL_VOLUME_SIGNATURE);

#define IFILE_FROM_FHAND(a)          CR (a, LKL_IFILE, Handle, LKL_IFILE_SIGNATURE)

#define ASSERT_VOLUME_LOCKED(a)      ASSERT_LOCKED (&LKLFsLock)

typedef struct _LKL_VOLUME {
  UINTN                           Signature;

  EFI_HANDLE                      Handle;
  BOOLEAN                         Valid;
  BOOLEAN                         DiskError;

  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL VolumeInterface;

  //
  // If opened, the parent handle and BlockIo interface
  //
  EFI_BLOCK_IO_PROTOCOL           *BlockIo;
  EFI_DISK_IO_PROTOCOL            *DiskIo;
  EFI_DISK_IO2_PROTOCOL           *DiskIo2;
  UINT32                          MediaId;
  BOOLEAN                         ReadOnly;

  struct lkl_disk                 LKLDisk;
  INTN                            LKLDiskId;
  CHAR8                           LKLMountPoint[32];
  CONST CHAR8                     *FsType;

  BOOLEAN                         IsEncrypted;
  CHAR8                           LKLBlkDevice[MAXPATHLEN];
  CHAR8                           LKLBlkDeviceDecrypted[MAXPATHLEN];
  CHAR8                           LKLCryptFSName[1024];
} LKL_VOLUME;

typedef struct {
  UINTN               Signature;
  EFI_FILE_PROTOCOL   Handle;

  LKL_VOLUME          *Volume;

  INTN                FD;
  INTN                LinuxOpenFlags;
  struct lkl_stat     StatBuf;
  CHAR8               FilePath[4096];

  struct lkl_dir      *Dir;
  struct lkl_linux_dirent64 *DirEnt;
} LKL_IFILE;

typedef enum {
  READ_DATA     = 0,
  WRITE_DATA    = 1
} IO_MODE;

//
// Global Variables
//
extern EFI_DRIVER_BINDING_PROTOCOL     gLKLDriverBinding;
extern EFI_COMPONENT_NAME_PROTOCOL     gLKLComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL    gLKLComponentName2;
extern EFI_LOCK                        LKLFsLock;
extern EFI_FILE_PROTOCOL               LKLFileInterface;

//
// Function Prototypes
//

void lkl_thread_init(void);
int cryptfs_setup_ext_volume(const char *label, const char *real_blkdev,
                             const unsigned char *key, int keysize, char *out_crypto_blkdev);
int cryptfs_revert_ext_volume(const char *label);

EFI_STATUS
GetFileFromAnyPartition (
  IN  CONST CHAR16                *Path,
  OUT EFI_FILE_PROTOCOL           **NewHandle
  );

VOID
EFIAPI
UnicodeToLower (
  IN EFI_STRING  UnicodeString
  );

EFI_STATUS
LKLError2EfiError (
  INTN Error
);

EFI_STATUS
LKLMakeDir (
 CONST CHAR8 *Path
);

CHAR8 *
RealPath (
  CONST CHAR8 *name,
  CHAR8 *resolved
  );

BOOLEAN
StartsWith (
  CONST CHAR8 *str,
  CONST CHAR8 *pre
  );

CONST
CHAR8*
GetBasenamePtr (
  CONST CHAR8 *s
  );

EFI_STATUS
InitializeUnicodeCollationSupport (
  IN EFI_HANDLE    AgentHandle
  );

VOID
LKLAcquireLock (
  VOID
  );

VOID
LKLReleaseLock (
  VOID
  );

CHAR8*
Unicode2Ascii (
  CONST CHAR16* UnicodeStr
);

CHAR16*
Ascii2Unicode (
  CONST CHAR8* AsciiStr
);

CHAR8*
AsciiStrDup (
  IN CONST CHAR8* Str
  );

CHAR16*
UnicodeStrDup (
  IN CONST CHAR16* Str
  );

VOID
PathToUnix(
  CHAR16* fname
);

VOID
PathToUefi(
  CHAR16* fname
);

VOID
PathToUnixAscii(
  CHAR8* fname
);

VOID
PathToUefiAscii(
  CHAR8* fname
);

VOID
EpochToEfiTime (
  IN  UINTN     EpochSeconds,
  OUT EFI_TIME  *Time
  );

UINTN
EfiTimeToEpoch (
  IN  EFI_TIME  *Time
  );

BOOLEAN
EfiTimeIsValid (
  IN EFI_TIME         *Time
  );

EFI_STATUS
LKLFillFileInfo (
  IN INTN             FD,
  OUT EFI_FILE_INFO   *FileInfo
  );

VOID
RemoveTrailingSlashes (
  CHAR8 *s
  );

EFI_STATUS
LKLAcquireLockOrFail (
  VOID
  );

VOID
LKLFreeVolume (
  IN LKL_VOLUME       *Volume
  );

//
// OpenVolume.c
//
EFI_STATUS
EFIAPI
LKLOpenVolume (
  IN  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
  OUT EFI_FILE_PROTOCOL               **File
  );

EFI_STATUS
LKLAllocateVolume (
  IN  EFI_HANDLE                Handle,
  IN  EFI_DISK_IO_PROTOCOL      *DiskIo,
  IN  EFI_DISK_IO2_PROTOCOL     *DiskIo2,
  IN  EFI_BLOCK_IO_PROTOCOL     *BlockIo
  );

EFI_STATUS
LKLAbandonVolume (
  IN LKL_VOLUME *Volume
  );

EFI_STATUS
LKLAllocateIFile (
  IN LKL_VOLUME   *Volume,
  IN INTN         FD,
  OUT LKL_IFILE   **PtrIFile
  );

EFI_STATUS
LKLIFileClose (
  LKL_IFILE           *IFile
  );


//
// Function Prototypes
//
EFI_STATUS
EFIAPI
LKLOpen (
  IN  EFI_FILE_PROTOCOL *FHand,
  OUT EFI_FILE_PROTOCOL **NewHandle,
  IN  CHAR16            *FileName,
  IN  UINT64            OpenMode,
  IN  UINT64            Attributes
  );

EFI_STATUS
EFIAPI
LKLOpenEx (
  IN  EFI_FILE_PROTOCOL       *FHand,
  OUT EFI_FILE_PROTOCOL       **NewHandle,
  IN  CHAR16                  *FileName,
  IN  UINT64                  OpenMode,
  IN  UINT64                  Attributes,
  IN OUT EFI_FILE_IO_TOKEN    *Token
  );

EFI_STATUS
EFIAPI
LKLGetPosition (
  IN  EFI_FILE_PROTOCOL *FHand,
  OUT UINT64            *Position
  );

EFI_STATUS
EFIAPI
LKLGetInfo (
  IN     EFI_FILE_PROTOCOL      *FHand,
  IN     EFI_GUID               *Type,
  IN OUT UINTN                  *BufferSize,
     OUT VOID                   *Buffer
  );

EFI_STATUS
EFIAPI
LKLSetInfo (
  IN EFI_FILE_PROTOCOL  *FHand,
  IN EFI_GUID           *Type,
  IN UINTN              BufferSize,
  IN VOID               *Buffer
  );

EFI_STATUS
EFIAPI
LKLFlush (
  IN EFI_FILE_PROTOCOL  *FHand
  );

EFI_STATUS
EFIAPI
LKLFlushEx (
  IN EFI_FILE_PROTOCOL  *FHand,
  IN EFI_FILE_IO_TOKEN  *Token
  );

EFI_STATUS
EFIAPI
LKLClose (
  IN EFI_FILE_PROTOCOL  *FHand
  );

EFI_STATUS
EFIAPI
LKLDelete (
  IN EFI_FILE_PROTOCOL  *FHand
  );

EFI_STATUS
EFIAPI
LKLSetPosition (
  IN EFI_FILE_PROTOCOL  *FHand,
  IN UINT64             Position
  );

EFI_STATUS
EFIAPI
LKLRead (
  IN     EFI_FILE_PROTOCOL    *FHand,
  IN OUT UINTN                *BufferSize,
     OUT VOID                 *Buffer
  );

EFI_STATUS
EFIAPI
LKLReadEx (
  IN     EFI_FILE_PROTOCOL  *FHand,
  IN OUT EFI_FILE_IO_TOKEN  *Token
  );

EFI_STATUS
EFIAPI
LKLWrite (
  IN     EFI_FILE_PROTOCOL      *FHand,
  IN OUT UINTN                  *BufferSize,
  IN     VOID                   *Buffer
  );

EFI_STATUS
EFIAPI
LKLWriteEx (
  IN     EFI_FILE_PROTOCOL  *FHand,
  IN OUT EFI_FILE_IO_TOKEN  *Token
  );

#endif
