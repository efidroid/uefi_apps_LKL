/*++

Copyright (c) 2005 - 2015, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2016, The EFIDroid Project. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available
under the terms and conditions of the BSD License which accompanies this
distribution. The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.


Module Name:

  ReadWrite.c

Abstract:

  Functions that perform file read/write

Revision History

--*/

#include "LKL.h"

EFI_STATUS
EFIAPI
LKLGetPosition (
  IN  EFI_FILE_PROTOCOL *FHand,
  OUT UINT64            *Position
  )
{
  LKL_IFILE  *IFile;
  LKL_VOLUME *Volume;
  lkl_loff_t NewPosition;
  EFI_STATUS Status;

  IFile = IFILE_FROM_FHAND (FHand);
  Volume  = IFile->Volume;
  (VOID)(Volume);

  NewPosition = lkl_sys_lseek(IFile->FD, 0, LKL_SEEK_CUR);

  if (NewPosition<0) {
    Status = LKLError2EfiError((INTN)NewPosition);
  }
  else {
    *Position = NewPosition;
    Status = EFI_SUCCESS;
  }

  return Status;
}

EFI_STATUS
EFIAPI
LKLSetPosition (
  IN EFI_FILE_PROTOCOL  *FHand,
  IN UINT64             Position
  )
{
  LKL_IFILE  *IFile;
  LKL_VOLUME *Volume;
  lkl_loff_t NewPosition;
  EFI_STATUS Status;

  IFile = IFILE_FROM_FHAND (FHand);
  Volume  = IFile->Volume;
  (VOID)(Volume);

  if (LKL_S_ISDIR(IFile->StatBuf.st_mode)) {
    if (Position==0) {
      if (IFile->Dir) {
        lkl_rewinddir(IFile->Dir);
      }
      IFile->DirEnt = NULL;
      Status = EFI_SUCCESS;
    }
    else {
      Status = EFI_UNSUPPORTED;
    }

    goto Done;
  }

  if (Position==0xffffffffffffffff) {
    NewPosition = lkl_sys_lseek(IFile->FD, 0, LKL_SEEK_END);
  }
  else {
    NewPosition = lkl_sys_lseek(IFile->FD, Position, LKL_SEEK_SET);
  }

  if (NewPosition<0)
    Status = LKLError2EfiError((INTN)NewPosition);
  else
    Status = EFI_SUCCESS;

Done:

  return Status;
}

EFI_STATUS
LKLIFileReadDir (
  IN     LKL_IFILE              *IFile,
  IN OUT UINTN                  *BufferSize,
     OUT VOID                   *Buffer
  )
{
  int         RC;
  struct lkl_linux_dirent64 *DirEnt;
  UINTN       Size;
  UINTN       NameSize;
  UINTN       ResultSize;
  EFI_FILE_INFO *FileInfo;

  FileInfo = Buffer;

  // open dir
  if (IFile->Dir == NULL) {
    IFile->Dir = lkl_fdopendir(IFile->FD, &RC);
    if (IFile->Dir == NULL)
      return LKLError2EfiError(RC);
  }

  if (IFile->DirEnt == NULL) {
    // read new entry
    DirEnt = lkl_readdir(IFile->Dir);
  }
  else {
    // use previously read entry
    DirEnt = IFile->DirEnt;
  }

  if (DirEnt == NULL) {
    // error
    if(lkl_errdir(IFile->Dir)) {
      *BufferSize = 0;
      return EFI_DEVICE_ERROR;
    }

    // EOF
    else {
      *BufferSize = 0;
      IFile->DirEnt = NULL;
      return EFI_SUCCESS;
    }
  }

  // calculate size
  Size = SIZE_OF_EFI_FILE_INFO;
  NameSize = AsciiStrSize(DirEnt->d_name) * sizeof(CHAR16);
  ResultSize = Size + NameSize;
  if (ResultSize > *BufferSize) {
    *BufferSize = ResultSize;
    IFile->DirEnt = DirEnt;
    return EFI_BUFFER_TOO_SMALL;
  }

  // initialize file info
  ZeroMem (FileInfo, SIZE_OF_EFI_FILE_INFO);
  FileInfo->Size = ResultSize;

  // write name
  AsciiStrToUnicodeStr(DirEnt->d_name, FileInfo->FileName);

  // add additional info
  INTN FD = lkl_sys_openat(IFile->FD, DirEnt->d_name, LKL_O_RDONLY, 0644);
  if (FD>=0) {
    LKLFillFileInfo(FD, FileInfo);
    lkl_sys_close(FD);
  }

  IFile->DirEnt = NULL;
  return EFI_SUCCESS;
}

EFI_STATUS
LKLIFileAccess (
  IN     EFI_FILE_PROTOCOL     *FHand,
  IN     IO_MODE               IoMode,
  IN OUT UINTN                 *BufferSize,
  IN OUT VOID                  *Buffer,
  IN     EFI_FILE_IO_TOKEN     *Token
  )
{
  EFI_STATUS  Status;
  LKL_IFILE   *IFile;
  LKL_VOLUME  *Volume;
  INTN        RC;

  IFile  = IFILE_FROM_FHAND (FHand);
  Volume = IFile->Volume;

  //
  // Write to a directory is unsupported
  //
  if (LKL_S_ISDIR(IFile->StatBuf.st_mode) && (IoMode == WRITE_DATA)) {
    return EFI_UNSUPPORTED;
  }

  if (IoMode == WRITE_DATA) {
    //
    // Check if the we can write data
    //
    if (Volume->ReadOnly) {
      return EFI_WRITE_PROTECTED;
    }
  }

  if (LKL_S_ISDIR(IFile->StatBuf.st_mode)) {
    //
    // Read a directory is supported
    //
    ASSERT (IoMode == READ_DATA);
    Status = LKLIFileReadDir (IFile, BufferSize, Buffer);
  } else {
    //
    // Access a file
    //
    
    if (IoMode == WRITE_DATA) {
      RC = lkl_sys_write(IFile->FD, Buffer, *BufferSize);
    }
    else {
      RC = lkl_sys_read(IFile->FD, Buffer, *BufferSize);
    }

    if(RC<0) {
      Status = LKLError2EfiError(RC);
    }
    else {
      *BufferSize = RC;
      Status = EFI_SUCCESS;
    }
  }

  return Status;
}

EFI_STATUS
EFIAPI
LKLRead (
  IN     EFI_FILE_PROTOCOL  *FHand,
  IN OUT UINTN              *BufferSize,
     OUT VOID               *Buffer
  )
{
  return LKLIFileAccess (FHand, READ_DATA, BufferSize, Buffer, NULL);
}

EFI_STATUS
EFIAPI
LKLReadEx (
  IN     EFI_FILE_PROTOCOL  *FHand,
  IN OUT EFI_FILE_IO_TOKEN  *Token
  )
{
  DEBUG((EFI_D_ERROR, "%a:%u\n", __func__, __LINE__));
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
LKLWrite (
  IN     EFI_FILE_PROTOCOL  *FHand,
  IN OUT UINTN              *BufferSize,
  IN     VOID               *Buffer
  )
{
  return LKLIFileAccess (FHand, WRITE_DATA, BufferSize, Buffer, NULL);
}

EFI_STATUS
EFIAPI
LKLWriteEx (
  IN     EFI_FILE_PROTOCOL  *FHand,
  IN OUT EFI_FILE_IO_TOKEN  *Token
  )
{
  DEBUG((EFI_D_ERROR, "%a:%u\n", __func__, __LINE__));
  return EFI_UNSUPPORTED;
}
