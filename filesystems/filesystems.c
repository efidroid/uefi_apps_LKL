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
