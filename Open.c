/*++

Copyright (c) 2005 - 2014, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2016, The EFIDroid Project. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available
under the terms and conditions of the BSD License which accompanies this
distribution. The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.


Module Name:

  open.c

Abstract:

  Routines dealing with file open

Revision History

--*/

#include "LKL.h"

EFI_STATUS
LKLAllocateIFile (
  IN LKL_VOLUME   *Volume,
  IN INTN         FD,
  OUT LKL_IFILE   **PtrIFile
  )
{
  LKL_IFILE         *IFile;
  EFI_STATUS        Status;
  INTN              RC;

  //
  // Allocate a new open instance
  //
  IFile = AllocateZeroPool (sizeof (LKL_IFILE));
  if (IFile == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  RC = lkl_sys_fstat(FD, &IFile->StatBuf);
  if (RC) {
    Status = LKLError2EfiError(RC);
    goto Done;
  }

  IFile->Signature = LKL_IFILE_SIGNATURE;
  IFile->FD        = FD;
  IFile->Volume    = Volume;

  CopyMem (&(IFile->Handle), &LKLFileInterface, sizeof (EFI_FILE_PROTOCOL));

  //
  // Report the correct revision number based on the DiskIo2 availability
  //
  IFile->Handle.Revision = EFI_FILE_PROTOCOL_REVISION;

  *PtrIFile = IFile;
  Status = EFI_SUCCESS;

Done:
  if (EFI_ERROR(Status)) {
    if (IFile)
      FreePool (IFile);
  }
  return Status;
}

EFI_STATUS
EFIAPI
LKLOpen (
  IN  EFI_FILE_PROTOCOL *FHand,
  OUT EFI_FILE_PROTOCOL **NewHandle,
  IN  CHAR16            *FileName,
  IN  UINT64            OpenMode,
  IN  UINT64            Attributes
  )
{
  EFI_STATUS  Status;
  INTN        RC;
  LKL_VOLUME  *Volume;
  LKL_IFILE   *ParentIFile;
  LKL_IFILE   *IFile;
  INTN        FD;
  CHAR8       *FileNameAscii;
  CHAR8       *NewFileName;
  CHAR8       *AbsFilePath;
  CONST CHAR8 *BaseName;
  BOOLEAN     ReadMode;
  BOOLEAN     WriteMode;
  BOOLEAN     CreateMode;
  INTN        LinuxFlags;
  lkl_umode_t LinuxMode;

  ParentIFile = IFILE_FROM_FHAND (FHand);
  Volume  = ParentIFile->Volume;
  FileNameAscii = NULL;
  NewFileName = NULL;
  AbsFilePath = NULL;
  BaseName = NULL;
  ReadMode =   ((OpenMode & EFI_FILE_MODE_READ)   !=0);
  WriteMode =  ((OpenMode & EFI_FILE_MODE_WRITE)  !=0);
  CreateMode = ((OpenMode & EFI_FILE_MODE_CREATE) !=0);
  LinuxFlags = 0;
  LinuxMode = 0;

  //
  // Perform some parameter checking
  //
  if (FileName == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Check for a valid mode
  //
  switch (OpenMode) {
  case EFI_FILE_MODE_READ:
  case EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE:
  case EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE:
    break;

  default:
    return EFI_INVALID_PARAMETER;
  }

  //
  // Check for valid Attributes for file creation case. 
  //
  if (((OpenMode & EFI_FILE_MODE_CREATE) != 0) && (Attributes & (EFI_FILE_READ_ONLY | (~EFI_FILE_VALID_ATTR))) != 0) {
    return EFI_INVALID_PARAMETER;
  }

  // generate access flags
  if (ReadMode && WriteMode)
    LinuxFlags |= LKL_O_RDWR;
  else if(ReadMode)
    LinuxFlags |= LKL_O_RDONLY;
  else if(WriteMode)
    LinuxFlags |= LKL_O_WRONLY;
  else
    return EFI_ACCESS_DENIED;

  if (CreateMode) {
    LinuxFlags |= LKL_O_CREAT;

    // generate mode flags
    if (Attributes & EFI_FILE_READ_ONLY)
      LinuxMode |= LKL_S_IRUSR; // u+r
    else
      LinuxMode |= LKL_S_IRUSR|LKL_S_IWUSR; // u+rw
  }

  FD = -1;

  if (Volume->ReadOnly && WriteMode) {
    return EFI_WRITE_PROTECTED;
  }

  // convert to char8 unix path
  FileNameAscii = Unicode2Ascii (FileName);
  if (FileNameAscii == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }
  PathToUnixAscii(FileNameAscii);

  // build absolute path(including the linux mountpoint)
  if (FileNameAscii[0]=='/') {
    UINTN  NewFileNameSize = AsciiStrLen(Volume->LKLMountPoint) + AsciiStrLen(FileNameAscii) + 1;
    NewFileName = AllocatePool(NewFileNameSize);
    if (!NewFileName) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Done;
    }

    AsciiSPrint(NewFileName, NewFileNameSize, "%a%a", Volume->LKLMountPoint, FileNameAscii);
  }
  else {
    UINTN  NewFileNameSize = AsciiStrLen(Volume->LKLMountPoint) + 1 + AsciiStrLen(ParentIFile->FilePath) + 1 + AsciiStrLen(FileNameAscii) + 1;
    NewFileName = AllocatePool(NewFileNameSize);
    if (!NewFileName) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Done;
    }

    AsciiSPrint(NewFileName, NewFileNameSize, "%a/%a/%a", Volume->LKLMountPoint, ParentIFile->FilePath, FileNameAscii);
  }

  if (CreateMode) {
    RemoveTrailingSlashes(NewFileName);
    BaseName = GetBasenamePtr(NewFileName);
    if (BaseName[0]==0) {
      Status = EFI_NOT_FOUND;
      goto Done;
    }

    // remove basename from 'NewFileName'
    ((CHAR8*)BaseName)[-1] = 0;
  }

  // resolve realpath
  AbsFilePath = RealPath(NewFileName, NULL);
  if (AbsFilePath==NULL) {
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  if (CreateMode) {
    UINTN  CreateNameSize = AsciiStrLen(AbsFilePath) + 1 + AsciiStrLen(BaseName) + 1;
    CHAR8 *CreateName = AllocatePool(CreateNameSize);
    if (CreateName == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Done;
    }

    AsciiSPrint(CreateName, CreateNameSize, "%a/%a", AbsFilePath, BaseName);

    FreePool(AbsFilePath);
    BaseName = NULL;
    AbsFilePath = CreateName;

    // use mkdir if we want to create a directory
    if (Attributes & EFI_FILE_DIRECTORY) {
      RC = lkl_sys_mkdir(AbsFilePath, LinuxMode);
      if (RC) {
        Status = LKLError2EfiError(RC);
        goto Done;
      }

      // change to RO open mode so open will not create a new file
      LinuxFlags = LKL_O_RDONLY;
      LinuxMode = 0;
    }
  }

  // return error if we're outside the mountpoint's root directory
  if (!StartsWith(AbsFilePath, Volume->LKLMountPoint)) {
    Status = EFI_NOT_FOUND;
    goto Done;
  }

  // open the file
  FD = lkl_sys_open(AbsFilePath, LinuxFlags, LinuxMode);
  if(FD<0) {
    Status = LKLError2EfiError(FD);
    goto Done;
  }

  // allocate internal file structure
  Status = LKLAllocateIFile (Volume, FD, &IFile);
  if (!EFI_ERROR (Status)) {

    CONST CHAR8* CopySrc = AbsFilePath + AsciiStrLen(Volume->LKLMountPoint);
    if (CopySrc[0]=='/')
      CopySrc++;

    AsciiStrnCpyS(IFile->FilePath, sizeof(IFile->FilePath), CopySrc, sizeof(IFile->FilePath));
    IFile->LinuxOpenFlags = LinuxFlags;
    *NewHandle = &IFile->Handle;
  }

Done:
  if (EFI_ERROR(Status)) {
    if (FD>=0)
      lkl_sys_close (FD);
  }

  if (FileNameAscii!=NULL)
    FreePool (FileNameAscii);
  if (NewFileName!=NULL)
    FreePool (NewFileName);
  if (AbsFilePath!=NULL)
    FreePool (AbsFilePath);

  return Status;
}

EFI_STATUS
EFIAPI
LKLOpenEx (
  IN  EFI_FILE_PROTOCOL       *FHand,
  OUT EFI_FILE_PROTOCOL       **NewHandle,
  IN  CHAR16                  *FileName,
  IN  UINT64                  OpenMode,
  IN  UINT64                  Attributes,
  IN OUT EFI_FILE_IO_TOKEN    *Token
  )
{
  DEBUG((EFI_D_ERROR, "%a:%u\n", __func__, __LINE__));
  return EFI_DEVICE_ERROR;
}

EFI_STATUS
LKLIFileClose (
  LKL_IFILE           *IFile
  )
{
  lkl_sys_close(IFile->FD);
  
  //
  // Done. Free the open instance structure
  //
  FreePool (IFile);
  return EFI_SUCCESS;
}
