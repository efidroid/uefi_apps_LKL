#ifndef _FSID_H
#define _FSID_H

typedef struct 
{
  CONST CHAR8    *magic; // magic string 
  UINTN          len;    // length of magic

  INTN           kboff;  // kilobyte offset of superblock
  UINTN          sboff;  // byte offset within superblock 
} FS_IDMAG;

typedef struct {
  CONST CHAR8 *Name;
  FS_IDMAG    Magics[];
} FS_IDINFO;

EFI_STATUS
GetFsType (
  IN  EFI_DISK_IO_PROTOCOL      *DiskIo,
  IN  UINT32                    MediaId,
  OUT CONST CHAR8               **OutName
  );

EFI_STATUS
GetFsTypeLKL (
  IN  CONST CHAR8               *Path,
  OUT CONST CHAR8               **OutName
  );

extern CONST FS_IDINFO IdInfoExt;
extern CONST FS_IDINFO IdInfoNTFS;
extern CONST FS_IDINFO IdInfoF2FS;

#endif
