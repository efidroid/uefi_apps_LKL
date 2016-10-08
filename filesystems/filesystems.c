#include "LKL.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

CONST FS_IDINFO* FsIdInfos[] = {
  &IdInfoExt,
  &IdInfoNTFS,
  &IdInfoF2FS,
};

EFI_STATUS
GetFsType (
  IN  EFI_DISK_IO_PROTOCOL      *DiskIo,
  IN  UINT32                    MediaId,
  OUT CONST CHAR8               **OutName
  )
{
  EFI_STATUS               Status;
  CONST FS_IDINFO          *IdInfo;
  CONST FS_IDMAG           *Magic;
  UINT64                   Offset;
  UINT8                    Buffer[1024];
  UINTN                    Index;

  for (Index=0; Index<ARRAY_SIZE(FsIdInfos); Index++) {
    IdInfo = FsIdInfos[Index];

    for(Magic=IdInfo->Magics; Magic->magic; Magic++) {
      // calculate offset
      Offset = (Magic->kboff + (Magic->sboff >> 10)) << 10;

      // read magic
      Status = DiskIo->ReadDisk(DiskIo, MediaId, Offset, sizeof(Buffer), Buffer);
      if(EFI_ERROR(Status)) {
        continue;
      }

      // compare magic
      if (!CompareMem(Magic->magic, Buffer + (Magic->sboff & 0x3ff), Magic->len)) {
        *OutName = IdInfo->Name;
        return EFI_SUCCESS;
      }
    }
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS
GetFsTypeLKL (
  IN  CONST CHAR8               *Path,
  OUT CONST CHAR8               **OutName
  )
{
  EFI_STATUS               Status;
  CONST FS_IDINFO          *IdInfo;
  CONST FS_IDMAG           *Magic;
  UINT64                   Offset;
  UINT8                    Buffer[1024];
  UINTN                    Index;
  INTN                     FD;
  INTN                     Ret;

  // open file
  FD = lkl_sys_open(Path, LKL_O_RDONLY, 0);
  if (FD<0) {
    return LKLError2EfiError(FD);
  }

  Status = EFI_NOT_FOUND;

  for (Index=0; Index<ARRAY_SIZE(FsIdInfos); Index++) {
    IdInfo = FsIdInfos[Index];

    for(Magic=IdInfo->Magics; Magic->magic; Magic++) {
      // calculate offset
      Offset = (Magic->kboff + (Magic->sboff >> 10)) << 10;

      // seek to offset
      Ret = lkl_sys_lseek(FD, Offset, LKL_SEEK_SET);
      if (Ret < 0) {
        continue;
      }

      // read magic
      Ret = lkl_sys_read(FD, (VOID*)Buffer, sizeof(Buffer));
      if(Ret<0) {
        continue;
      }

      // compare magic
      if (!CompareMem(Magic->magic, Buffer + (Magic->sboff & 0x3ff), Magic->len)) {
        *OutName = IdInfo->Name;
        Status = EFI_SUCCESS;
        goto Done;
      }
    }
  }

Done:
  // close file
  lkl_sys_close(FD);

  return Status;
}
