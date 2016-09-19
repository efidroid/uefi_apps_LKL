/*++

Copyright (c) 2005 - 2015, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available
under the terms and conditions of the BSD License which accompanies this
distribution. The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.


Module Name:

  Info.c

Abstract:

  Routines dealing with setting/getting file/volume info

Revision History

--*/

#include "LKL.h"

EFI_STATUS
LKLGetVolumeInfo (
  IN LKL_VOLUME       *Volume,
  IN OUT UINTN        *BufferSize,
  OUT VOID            *Buffer
  );

EFI_STATUS
LKLSetVolumeInfo (
  IN LKL_VOLUME       *Volume,
  IN OUT UINTN        BufferSize,
  OUT VOID            *Buffer
  );

EFI_STATUS
LKLSetOrGetInfo (
  IN BOOLEAN              IsSet,
  IN EFI_FILE_PROTOCOL    *FHand,
  IN EFI_GUID             *Type,
  IN OUT UINTN            *BufferSize,
  IN OUT VOID             *Buffer
  );

EFI_STATUS
LKLGetFileInfo (
  IN LKL_IFILE        *IFile,
  IN OUT UINTN        *BufferSize,
  OUT VOID            *Buffer
  )
{
  UINTN       Size;
  UINTN       NameSize;
  UINTN       ResultSize;
  EFI_FILE_INFO *FileInfo;
  CONST CHAR8   *FileName;

  FileInfo = Buffer;
  FileName = GetBasenamePtr(IFile->FilePath);

  // calculate size
  Size = SIZE_OF_EFI_FILE_INFO;
  NameSize = AsciiStrSize(FileName)*sizeof(CHAR16);
  ResultSize = Size + NameSize;
  if (ResultSize > *BufferSize) {
    *BufferSize = ResultSize;
    return EFI_BUFFER_TOO_SMALL;
  }

  // initialize file info
  ZeroMem (FileInfo, SIZE_OF_EFI_FILE_INFO);
  FileInfo->Size = ResultSize;

  // write name
  AsciiStrToUnicodeStr(FileName, FileInfo->FileName);

  // add additional info
  LKLFillFileInfo(IFile->FD, FileInfo);

  if ((IFile->LinuxOpenMode&LKL_S_IWUSR)==0)
    FileInfo->Attribute |= EFI_FILE_READ_ONLY;

  if (LKL_S_ISDIR(IFile->StatBuf.st_mode))
    FileInfo->Attribute |= EFI_FILE_DIRECTORY;

  return EFI_SUCCESS;
}

EFI_STATUS
LKLGetVolumeInfo (
  IN     LKL_VOLUME     *Volume,
  IN OUT UINTN          *BufferSize,
     OUT VOID           *Buffer
  )
{
  UINTN                 Size;
  UINTN                 NameSize;
  UINTN                 ResultSize;
  EFI_STATUS            Status;
  EFI_FILE_SYSTEM_INFO  *Info;
  struct lkl_statfs64   StatBuf;
  INTN                  RC;

  RC = lkl_sys_statfs64(Volume->LKLMountPoint, sizeof(StatBuf), &StatBuf);
  if (RC) {
    return EFI_UNSUPPORTED;
  }

  Size              = SIZE_OF_EFI_FILE_SYSTEM_INFO;
  NameSize          = 2;
  ResultSize        = Size + NameSize;

  Status = EFI_BUFFER_TOO_SMALL;
  if (*BufferSize >= ResultSize) {
    Status  = EFI_SUCCESS;

    Info    = Buffer;
    ZeroMem (Info, SIZE_OF_EFI_FILE_SYSTEM_INFO);

    Info->Size        = ResultSize;
    Info->ReadOnly    = Volume->ReadOnly;
    Info->BlockSize   = StatBuf.f_bsize;
    Info->VolumeSize  = StatBuf.f_blocks * Info->BlockSize;
    Info->FreeSpace   = StatBuf.f_bfree  * Info->BlockSize;
    SetMem (Buffer + Size, NameSize, '\0');
  }

  *BufferSize = ResultSize;
  return Status;
}

#if 0
EFI_STATUS
LKLGetVolumeLabelInfo (
  IN LKL_VOLUME       *Volume,
  IN OUT UINTN        *BufferSize,
  OUT VOID            *Buffer
  )
{
  UINTN                             Size;
  UINTN                             NameSize;
  UINTN                             ResultSize;
  CHAR16                            Name[LKL_NAME_LEN + 1];
  EFI_STATUS                        Status;

  Size        = SIZE_OF_EFI_FILE_SYSTEM_VOLUME_LABEL;
  Status      = LKLGetVolumeEntry (Volume, Name);
  NameSize    = StrSize (Name);
  ResultSize  = Size + NameSize;

  Status      = EFI_BUFFER_TOO_SMALL;
  if (*BufferSize >= ResultSize) {
    Status  = EFI_SUCCESS;
    CopyMem ((CHAR8 *) Buffer + Size, Name, NameSize);
  }

  *BufferSize = ResultSize;
  return Status;
}
#endif

#if 0
EFI_STATUS
LKLSetVolumeInfo (
  IN LKL_VOLUME       *Volume,
  IN UINTN            BufferSize,
  IN VOID             *Buffer
  )
{
  EFI_FILE_SYSTEM_INFO  *Info;

  Info = (EFI_FILE_SYSTEM_INFO *) Buffer;

  if (BufferSize < SIZE_OF_EFI_FILE_SYSTEM_INFO + 2 || Info->Size > BufferSize) {
    return EFI_BAD_BUFFER_SIZE;
  }

  return LKLSetVolumeEntry (Volume, Info->VolumeLabel);
}
#endif

#if 0
EFI_STATUS
LKLSetVolumeLabelInfo (
  IN LKL_VOLUME       *Volume,
  IN UINTN            BufferSize,
  IN VOID             *Buffer
  )
{
  EFI_FILE_SYSTEM_VOLUME_LABEL *Info;

  Info = (EFI_FILE_SYSTEM_VOLUME_LABEL *) Buffer;

  if (BufferSize < SIZE_OF_EFI_FILE_SYSTEM_VOLUME_LABEL + 2) {
    return EFI_BAD_BUFFER_SIZE;
  }

  return LKLSetVolumeEntry (Volume, Info->VolumeLabel);
}
#endif

#if 0
EFI_STATUS
LKLSetFileInfo (
  IN LKL_VOLUME       *Volume,
  IN LKL_IFILE        *IFile,
  IN UINTN            BufferSize,
  IN VOID             *Buffer
  )
{
  EFI_STATUS    Status;
  EFI_FILE_INFO *NewInfo;
  LKL_OFILE     *DotOFile;
  LKL_OFILE     *Parent;
  CHAR16        NewFileName[EFI_PATH_STRING_LENGTH];
  EFI_TIME      ZeroTime;
  UINT8         NewAttribute;
  BOOLEAN       ReadOnly;

  ZeroMem (&ZeroTime, sizeof (EFI_TIME));
  Parent  = OFile->Parent;
  //
  // If this is the root directory, we can't make any updates
  //
  if (IFile->FilePath[0]==0) {
    return EFI_ACCESS_DENIED;
  }
  //
  // Make sure there's a valid input buffer
  //
  NewInfo = Buffer;
  if (BufferSize < SIZE_OF_EFI_FILE_INFO + 2 || NewInfo->Size > BufferSize) {
    return EFI_BAD_BUFFER_SIZE;
  }

  ReadOnly = (BOOLEAN)(IFile->ReadOnly || (DirEnt->Entry.Attributes & EFI_FILE_READ_ONLY));
  //
  // if a zero time is specified, then the original time is preserved
  //
  if (CompareMem (&ZeroTime, &NewInfo->CreateTime, sizeof (EFI_TIME)) != 0) {
    if (!LKLIsValidTime (&NewInfo->CreateTime)) {
      return EFI_INVALID_PARAMETER;
    }

    if (!ReadOnly) {
      LKLEfiTimeToLKLTime (&NewInfo->CreateTime, &DirEnt->Entry.FileCreateTime);
    }
  }

  if (CompareMem (&ZeroTime, &NewInfo->ModificationTime, sizeof (EFI_TIME)) != 0) {
    if (!LKLIsValidTime (&NewInfo->ModificationTime)) {
      return EFI_INVALID_PARAMETER;
    }

    if (!ReadOnly) {
      LKLEfiTimeToLKLTime (&NewInfo->ModificationTime, &DirEnt->Entry.FileModificationTime);
    }

    OFile->PreserveLastModification = TRUE;
  }

  if (NewInfo->Attribute & (~EFI_FILE_VALID_ATTR)) {
    return EFI_INVALID_PARAMETER;
  }

  NewAttribute = (UINT8) NewInfo->Attribute;
  //
  // Can not change the directory attribute bit
  //
  if ((NewAttribute ^ DirEnt->Entry.Attributes) & EFI_FILE_DIRECTORY) {
    return EFI_ACCESS_DENIED;
  }
  //
  // Set the current attributes even if the IFile->ReadOnly is TRUE
  //
  DirEnt->Entry.Attributes = (UINT8) ((DirEnt->Entry.Attributes &~EFI_FILE_VALID_ATTR) | NewAttribute);
  //
  // Open the filename and see if it refers to an existing file
  //
  Status = LKLLocateOFile (&Parent, NewInfo->FileName, DirEnt->Entry.Attributes, NewFileName);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (*NewFileName != 0) {
    //
    // File was not found.  We do not allow rename of the current directory if
    // there are open files below the current directory
    //
    if (!IsListEmpty (&OFile->ChildHead) || Parent == OFile) {
      return EFI_ACCESS_DENIED;
    }

    if (ReadOnly) {
      return EFI_ACCESS_DENIED;
    }

    Status = LKLRemoveDirEnt (OFile->Parent, DirEnt);
    if (EFI_ERROR (Status)) {
      return Status;
    }
    //
    // Create new dirent
    //
    Status = LKLCreateDirEnt (Parent, NewFileName, DirEnt->Entry.Attributes, &TempDirEnt);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    LKLCloneDirEnt (TempDirEnt, DirEnt);
    LKLFreeDirEnt (DirEnt);
    DirEnt        = TempDirEnt;
    DirEnt->OFile = OFile;
    OFile->DirEnt = DirEnt;
    OFile->Parent = Parent;
    RemoveEntryList (&OFile->ChildLink);
    InsertHeadList (&Parent->ChildHead, &OFile->ChildLink);
    //
    // If this is a directory, synchronize its dot directory entry
    //
    if (OFile->ODir != NULL) {
      //
      // Syncronize its dot entry
      //
      LKLResetODirCursor (OFile);
      ASSERT (OFile->Parent != NULL);
      for (DotOFile = OFile; DotOFile != OFile->Parent->Parent; DotOFile = DotOFile->Parent) {
        Status = LKLGetNextDirEnt (OFile, &DirEnt);
        if (EFI_ERROR (Status) || DirEnt == NULL || !LKLIsDotDirEnt (DirEnt)) {
          return EFI_VOLUME_CORRUPTED;
        }

        LKLCloneDirEnt (DirEnt, DotOFile->DirEnt);
        Status = LKLStoreDirEnt (OFile, DirEnt);
        if (EFI_ERROR (Status)) {
          return Status;
        }
      }
    }
    //
    // If the file is renamed, we should append the ARCHIVE attribute
    //
    OFile->Archive = TRUE;
  } else if (Parent != OFile) {
    //
    // filename is to a different filename that already exists
    //
    return EFI_ACCESS_DENIED;
  }
  //
  // If the file size has changed, apply it
  //
  if (NewInfo->FileSize != OFile->FileSize) {
    if (OFile->ODir != NULL || ReadOnly) {
      //
      // If this is a directory or the file is read only, we can't change the file size
      //
      return EFI_ACCESS_DENIED;
    }

    if (NewInfo->FileSize > OFile->FileSize) {
      Status = LKLExpandOFile (OFile, NewInfo->FileSize);
    } else {
      Status = LKLTruncateOFile (OFile, (UINTN) NewInfo->FileSize);
    }

    if (EFI_ERROR (Status)) {
      return Status;
    }

    LKLUpdateDirEntClusterSizeInfo (OFile);
  }

  OFile->Dirty = TRUE;
  return LKLOFileFlush (OFile);
}
#endif

EFI_STATUS
LKLSetOrGetInfo (
  IN     BOOLEAN            IsSet,
  IN     EFI_FILE_PROTOCOL  *FHand,
  IN     EFI_GUID           *Type,
  IN OUT UINTN              *BufferSize,
  IN OUT VOID               *Buffer
  )
{
  LKL_IFILE   *IFile;
  LKL_VOLUME  *Volume;
  EFI_STATUS  Status;

  IFile   = IFILE_FROM_FHAND (FHand);
  Volume  = IFile->Volume;

  LKLAcquireLock ();

  //
  // Get the proper information based on the request
  //
  Status = EFI_UNSUPPORTED;
  if (IsSet) {
#if 0
    if (CompareGuid (Type, &gEfiFileInfoGuid)) {
      Status = Volume->ReadOnly ? EFI_WRITE_PROTECTED : LKLSetFileInfo (Volume, IFile, *BufferSize, Buffer);
    }
#endif

#if 0
    if (CompareGuid (Type, &gEfiFileSystemInfoGuid)) {
      Status = Volume->ReadOnly ? EFI_WRITE_PROTECTED : LKLSetVolumeInfo (Volume, *BufferSize, Buffer);
    }
#endif

#if 0
    if (CompareGuid (Type, &gEfiFileSystemVolumeLabelInfoIdGuid)) {
      Status = Volume->ReadOnly ? EFI_WRITE_PROTECTED : LKLSetVolumeLabelInfo (Volume, *BufferSize, Buffer);
    }
#endif
  } else {
    if (CompareGuid (Type, &gEfiFileInfoGuid)) {
      Status = LKLGetFileInfo (IFile, BufferSize, Buffer);
    }

    if (CompareGuid (Type, &gEfiFileSystemInfoGuid)) {
      Status = LKLGetVolumeInfo (Volume, BufferSize, Buffer);
    }

#if 0
    if (CompareGuid (Type, &gEfiFileSystemVolumeLabelInfoIdGuid)) {
      Status = LKLGetVolumeLabelInfo (Volume, BufferSize, Buffer);
    }
#endif
  }

  LKLReleaseLock ();
  return Status;
}

EFI_STATUS
EFIAPI
LKLGetInfo (
  IN     EFI_FILE_PROTOCOL   *FHand,
  IN     EFI_GUID            *Type,
  IN OUT UINTN               *BufferSize,
     OUT VOID                *Buffer
  )
{
  return LKLSetOrGetInfo (FALSE, FHand, Type, BufferSize, Buffer);
}

EFI_STATUS
EFIAPI
LKLSetInfo (
  IN EFI_FILE_PROTOCOL  *FHand,
  IN EFI_GUID           *Type,
  IN UINTN              BufferSize,
  IN VOID               *Buffer
  )
{
  return LKLSetOrGetInfo (TRUE, FHand, Type, &BufferSize, Buffer);
}
