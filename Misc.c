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

  Misc.c

Abstract:

  Miscellaneous functions

Revision History

--*/

#include "LKL.h"

#define __alloca(size) __builtin_alloca (size)

VOID
LKLAcquireLock (
  VOID
  )
{
  EfiAcquireLock (&LKLFsLock);
}

EFI_STATUS
LKLAcquireLockOrFail (
  VOID
  )
{
  return EfiAcquireLockOrFail (&LKLFsLock);
}

VOID
LKLReleaseLock (
  VOID
  )
{
  EfiReleaseLock (&LKLFsLock);
}

VOID
LKLFreeVolume (
  IN LKL_VOLUME       *Volume
  )
{
  
  FreePool (Volume);
}

CHAR8*
Unicode2Ascii (
  CONST CHAR16* UnicodeStr
)
{
  CHAR8* AsciiStr = AllocatePool((StrLen (UnicodeStr) + 1) * sizeof (CHAR8));
  if (AsciiStr == NULL) {
    return NULL;
  }

  UnicodeStrToAsciiStr(UnicodeStr, AsciiStr);

  return AsciiStr;
}

CHAR16*
Ascii2Unicode (
  CONST CHAR8* AsciiStr
)
{
  CHAR16* UnicodeStr = AllocatePool((AsciiStrLen (AsciiStr) + 1) * sizeof (CHAR16));
  if (UnicodeStr == NULL) {
    return NULL;
  }

  AsciiStrToUnicodeStr(AsciiStr, UnicodeStr);

  return UnicodeStr;
}

CHAR8*
AsciiStrDup (
  IN CONST CHAR8* Str
  )
{
  return AllocateCopyPool (AsciiStrSize (Str), Str);
}

CHAR16*
UnicodeStrDup (
  IN CONST CHAR16* Str
  )
{
  return AllocateCopyPool (StrSize (Str), Str);
}

VOID
PathToUnix(
  CHAR16* fname
)
{
  CHAR16 *Tmp = fname;
  for(Tmp = fname; *Tmp != 0; Tmp++) {
    if(*Tmp=='\\')
      *Tmp = '/';
  }
}

VOID
PathToUefi(
  CHAR16* fname
)
{
  CHAR16 *Tmp = fname;
  for(Tmp = fname; *Tmp != 0; Tmp++) {
    if(*Tmp=='/')
      *Tmp = '\\';
  }
}

VOID
PathToUnixAscii(
  CHAR8* fname
)
{
  CHAR8 *Tmp = fname;
  for(Tmp = fname; *Tmp != 0; Tmp++) {
    if(*Tmp=='\\')
      *Tmp = '/';
  }
}

VOID
PathToUefiAscii(
  CHAR8* fname
)
{
  CHAR8 *Tmp = fname;
  for(Tmp = fname; *Tmp != 0; Tmp++) {
    if(*Tmp=='/')
      *Tmp = '\\';
  }
}

STATIC VOID*
mempcpy (
  VOID *dest,
  CONST VOID *src,
  UINTN n
  )
{
  return (CHAR8 *)CopyMem(dest, src, n) + n;
}

CHAR8 *
RealPath (
  CONST CHAR8 *name,
  CHAR8 *resolved
  )
{
  CHAR8 *rpath, *dest, *extra_buf = NULL;
  CONST CHAR8 *start, *end, *rpath_limit;
  INTN path_max;
  INTN num_links = 0;
  UINTN AllocSize;

  if (name == NULL) {
    /* As per Single Unix Specification V2 we must return an error if
    either parameter is a null pointer.  We extend this to allow
     the RESOLVED parameter to be NULL in case the we are expected to
     allocate the room for the return value.  */
    return NULL;
  }

  if (name[0] == '\0') {
    /* As per Single Unix Specification V2 we must return an error if
    the name argument points to an empty string.  */
    return NULL;
  }

  path_max = 4096;

  if (resolved == NULL) {
    AllocSize = path_max;
    rpath = AllocatePool (path_max);
    if (rpath == NULL)
      return NULL;
  } else
    rpath = resolved;
  rpath_limit = rpath + path_max;

  if (name[0] != '/') {
    goto error;
  } else {
    rpath[0] = '/';
    dest = rpath + 1;
  }

  for (start = end = name; *start; start = end) {
    struct lkl_stat64 st;
    INTN n;

    /* Skip sequence of multiple path-separators.  */
    while (*start == '/')
      ++start;

    /* Find end of path component.  */
    for (end = start; *end && *end != '/'; ++end)
      /* Nothing.  */;

    if (end - start == 0)
      break;
    else if (end - start == 1 && start[0] == '.')
      /* nothing */;
    else if (end - start == 2 && start[0] == '.' && start[1] == '.') {
      /* Back up to previous component, ignore if at root already.  */
      if (dest > rpath + 1)
        while ((--dest)[-1] != '/');
    } else {
      UINTN new_size;

      if (dest[-1] != '/')
        *dest++ = '/';

      if (dest + (end - start) >= rpath_limit) {
        INTN dest_offset = dest - rpath;
        CHAR8 *new_rpath;

        if (resolved) {
          if (dest > rpath + 1)
            dest--;
          *dest = '\0';
          goto error;
        }
        new_size = rpath_limit - rpath;
        if (end - start + 1 > path_max)
          new_size += end - start + 1;
        else
          new_size += path_max;
        new_rpath = (CHAR8 *) ReallocatePool (AllocSize, new_size, rpath);
        AllocSize = new_size;
        if (new_rpath == NULL)
          goto error;
        rpath = new_rpath;
        rpath_limit = rpath + new_size;

        dest = rpath + dest_offset;
      }

      dest = mempcpy (dest, start, end - start);
      *dest = '\0';

      if (lkl_sys_lstat64 (rpath, &st) < 0)
        goto error;

      if (LKL_S_ISLNK (st.st_mode)) {
        CHAR8 *buf = __alloca (path_max);
        UINTN len;

        if (++num_links > 40) {
          goto error;
        }

        n = lkl_sys_readlink (rpath, buf, path_max - 1);
        if (n < 0)
          goto error;
        buf[n] = '\0';

        if (!extra_buf)
          extra_buf = __alloca (path_max);

        len = AsciiStrLen (end);
        if ((INTN) (n + len) >= path_max) {
          goto error;
        }

        /* Careful here, end may be a pointer into extra_buf... */
        CopyMem (&extra_buf[n], end, len + 1);
        name = end = CopyMem (extra_buf, buf, n);

        if (buf[0] == '/')
          dest = rpath + 1;   /* It's an absolute symlink */
        else
          /* Back up to previous component, ignore if at root already: */
          if (dest > rpath + 1)
            while ((--dest)[-1] != '/');
      }
    }
  }
  if (dest > rpath + 1 && dest[-1] == '/')
    --dest;
  *dest = '\0';

  ASSERT (resolved == NULL || resolved == rpath);
  return rpath;

error:
  ASSERT (resolved == NULL || resolved == rpath);
  if (resolved == NULL)
    FreePool (rpath);
  return NULL;
}

CONST
CHAR8*
GetBasenamePtr (
  CONST CHAR8 *s
  )
{
  UINTN i;

  if (!s || !*s)
    return s;

  i = AsciiStrLen(s)-1;

  for (; i && s[i-1]!='/'; i--);

  return s+i;
}

/**
  Converts Epoch seconds (elapsed since 1970 JANUARY 01, 00:00:00 UTC) to EFI_TIME
 **/
VOID
EpochToEfiTime (
  IN  UINTN     EpochSeconds,
  OUT EFI_TIME  *Time
  )
{
  UINTN         a;
  UINTN         b;
  UINTN         c;
  UINTN         d;
  UINTN         g;
  UINTN         j;
  UINTN         m;
  UINTN         y;
  UINTN         da;
  UINTN         db;
  UINTN         dc;
  UINTN         dg;
  UINTN         hh;
  UINTN         mm;
  UINTN         ss;
  UINTN         J;

  J  = (EpochSeconds / 86400) + 2440588;
  j  = J + 32044;
  g  = j / 146097;
  dg = j % 146097;
  c  = (((dg / 36524) + 1) * 3) / 4;
  dc = dg - (c * 36524);
  b  = dc / 1461;
  db = dc % 1461;
  a  = (((db / 365) + 1) * 3) / 4;
  da = db - (a * 365);
  y  = (g * 400) + (c * 100) + (b * 4) + a;
  m  = (((da * 5) + 308) / 153) - 2;
  d  = da - (((m + 4) * 153) / 5) + 122;

  Time->Year  = y - 4800 + ((m + 2) / 12);
  Time->Month = ((m + 2) % 12) + 1;
  Time->Day   = d + 1;

  ss = EpochSeconds % 60;
  a  = (EpochSeconds - ss) / 60;
  mm = a % 60;
  b = (a - mm) / 60;
  hh = b % 24;

  Time->Hour        = hh;
  Time->Minute      = mm;
  Time->Second      = ss;
  Time->Nanosecond  = 0;

}

// Define EPOCH (1970-JANUARY-01) in the Julian Date representation
#define EPOCH_JULIAN_DATE                               2440588

// Seconds per unit
#define SEC_PER_MIN                                     ((UINTN)    60)
#define SEC_PER_HOUR                                    ((UINTN)  3600)
#define SEC_PER_DAY                                     ((UINTN) 86400)

#define SEC_PER_MONTH                                   ((UINTN)  2,592,000)
#define SEC_PER_YEAR                                    ((UINTN) 31,536,000)

/**
  Converts EFI_TIME to Epoch seconds (elapsed since 1970 JANUARY 01, 00:00:00 UTC)
 **/
UINTN
EfiTimeToEpoch (
  IN  EFI_TIME  *Time
  )
{
  UINTN a;
  UINTN y;
  UINTN m;
  UINTN JulianDate;  // Absolute Julian Date representation of the supplied Time
  UINTN EpochDays;   // Number of days elapsed since EPOCH_JULIAN_DAY
  UINTN EpochSeconds;

  a = (14 - Time->Month) / 12 ;
  y = Time->Year + 4800 - a;
  m = Time->Month + (12*a) - 3;

  JulianDate = Time->Day + ((153*m + 2)/5) + (365*y) + (y/4) - (y/100) + (y/400) - 32045;

  ASSERT (JulianDate >= EPOCH_JULIAN_DATE);
  EpochDays = JulianDate - EPOCH_JULIAN_DATE;

  EpochSeconds = (EpochDays * SEC_PER_DAY) + ((UINTN)Time->Hour * SEC_PER_HOUR) + (Time->Minute * SEC_PER_MIN) + Time->Second;

  return EpochSeconds;
}

BOOLEAN
IsLeapYear (
  IN EFI_TIME   *Time
  )
{
  if (Time->Year % 4 == 0) {
    if (Time->Year % 100 == 0) {
      if (Time->Year % 400 == 0) {
        return TRUE;
      } else {
        return FALSE;
      }
    } else {
      return TRUE;
    }
  } else {
    return FALSE;
  }
}

BOOLEAN
DayValid (
  IN  EFI_TIME  *Time
  )
{
  STATIC CONST INTN DayOfMonth[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

  if (Time->Day < 1 ||
      Time->Day > DayOfMonth[Time->Month - 1] ||
      (Time->Month == 2 && (!IsLeapYear (Time) && Time->Day > 28))
     ) {
    return FALSE;
  }

  return TRUE;
}

BOOLEAN
EfiTimeIsValid (
  IN EFI_TIME         *Time
  )
{
  // Check the input parameters are within the range specified by UEFI
  if ((Time->Year   < 1900) ||
       (Time->Year   > 9999) ||
       (Time->Month  < 1   ) ||
       (Time->Month  > 12  ) ||
       (!DayValid (Time)    ) ||
       (Time->Hour   > 23  ) ||
       (Time->Minute > 59  ) ||
       (Time->Second > 59  ) ||
       (Time->Nanosecond > 999999999) ||
       (!((Time->TimeZone == EFI_UNSPECIFIED_TIMEZONE) || ((Time->TimeZone >= -1440) && (Time->TimeZone <= 1440)))) ||
       (Time->Daylight & (~(EFI_TIME_ADJUST_DAYLIGHT | EFI_TIME_IN_DAYLIGHT)))
    ) {
    return FALSE;
  }

  return TRUE;
}

EFI_STATUS
LKLFillFileInfo (
  IN INTN             FD,
  OUT EFI_FILE_INFO   *FileInfo
  )
{
  INTN              RC;
  struct lkl_stat64 StatBuf;

  RC = lkl_sys_fstat64(FD, &StatBuf);
  if (RC) {
    return  EFI_DEVICE_ERROR;
  }

  FileInfo->FileSize = StatBuf.st_size;
  FileInfo->PhysicalSize = StatBuf.st_blocks*512;
  EpochToEfiTime(StatBuf.lkl_st_atime, &FileInfo->LastAccessTime);
  EpochToEfiTime(StatBuf.lkl_st_mtime, &FileInfo->ModificationTime);

  // since there's no creation-time, use the modificationtime
  EpochToEfiTime(StatBuf.lkl_st_mtime, &FileInfo->CreateTime);
  FileInfo->Attribute = 0;

  if ((StatBuf.st_mode&LKL_S_IWUSR)==0)
    FileInfo->Attribute |= EFI_FILE_READ_ONLY;

  if (LKL_S_ISDIR(StatBuf.st_mode))
    FileInfo->Attribute |= EFI_FILE_DIRECTORY;

  return EFI_SUCCESS;
}

VOID
RemoveTrailingSlashes (
  CHAR8 *s
  )
{
  UINTN i;

  if (!s || !*s)
    return;

  i = AsciiStrLen(s)-1;
  for (; i && s[i]=='/'; i--)
    s[i] = 0;
}

BOOLEAN
StartsWith (
  CONST CHAR8 *str,
  CONST CHAR8 *pre
  )
{
    return AsciiStrnCmp(pre, str, AsciiStrLen(pre)) == 0;
}

EFI_STATUS
LKLMakeDir (
 CONST CHAR8 *Path
)
{
  INTN Ret;
  Ret = lkl_sys_mkdir(Path, 0700);
  if (Ret && Ret != -LKL_EEXIST) {
    return LKLError2EfiError(Ret);
  }

  return EFI_SUCCESS;
}

static EFI_STATUS CONST LKLErrorStatus[] = {
  EFI_SUCCESS,
  EFI_ACCESS_DENIED, // LKL_EPERM
  EFI_NOT_FOUND, // LKL_ENOENT
  EFI_NOT_FOUND, // LKL_ESRCH
  EFI_ABORTED, // LKL_EINTR
  EFI_DEVICE_ERROR, // LKL_EIO
  EFI_NOT_FOUND, // LKL_ENXIO
  EFI_INVALID_PARAMETER, // LKL_E2BIG
  EFI_INVALID_PARAMETER, // LKL_ENOEXEC
  EFI_INVALID_PARAMETER, // LKL_EBADF

  EFI_NOT_FOUND, // LKL_ECHILD
  EFI_NOT_READY, // LKL_EAGAIN
  EFI_OUT_OF_RESOURCES, // LKL_ENOMEM
  EFI_ACCESS_DENIED, // LKL_EACCES
  EFI_INVALID_PARAMETER, // LKL_EFAULT
  EFI_INVALID_PARAMETER, // LKL_ENOTBLK
  EFI_DEVICE_ERROR, // LKL_EBUSY
  EFI_DEVICE_ERROR, // LKL_EEXIST
  EFI_DEVICE_ERROR, // LKL_EXDEV
  EFI_NOT_FOUND, // LKL_ENODEV

  EFI_INVALID_PARAMETER, // LKL_ENOTDIR
  EFI_INVALID_PARAMETER, // LKL_EISDIR
  EFI_INVALID_PARAMETER, // LKL_EINVAL
  EFI_OUT_OF_RESOURCES, // LKL_ENFILE
  EFI_OUT_OF_RESOURCES, // LKL_EMFILE
  EFI_INVALID_PARAMETER, // LKL_ENOTTY
  EFI_NOT_READY, // LKL_ETXTBSY
  EFI_OUT_OF_RESOURCES, // LKL_EFBIG
  EFI_VOLUME_FULL, // LKL_ENOSPC
  EFI_INVALID_PARAMETER, // LKL_ESPIPE

  EFI_WRITE_PROTECTED, // LKL_EROFS
  EFI_OUT_OF_RESOURCES, // LKL_EMLINK
  EFI_DEVICE_ERROR, // LKL_EPIPE
  EFI_INVALID_PARAMETER, // LKL_EDOM
  EFI_UNSUPPORTED, // LKL_ERANGE
  EFI_UNSUPPORTED, // LKL_EDEADLK
  EFI_INVALID_PARAMETER, // LKL_ENAMETOOLONG
  EFI_OUT_OF_RESOURCES, // LKL_ENOLCK
  EFI_INVALID_PARAMETER, // LKL_ENOSYS
  EFI_UNSUPPORTED, // LKL_ENOTEMPTY

  EFI_DEVICE_ERROR, // LKL_ELOOP
  EFI_UNSUPPORTED,  // LKL_EWOULDBLOCK
  EFI_UNSUPPORTED, // LKL_ENOMSG
  EFI_INVALID_PARAMETER, // LKL_EIDRM
  EFI_INVALID_PARAMETER, // LKL_ECHRNG
  EFI_DEVICE_ERROR, // LKL_EL2NSYNC
  EFI_DEVICE_ERROR, // LKL_EL3HLT
  EFI_DEVICE_ERROR, // LKL_EL3RST
  EFI_INVALID_PARAMETER, // LKL_ELNRNG
  EFI_DEVICE_ERROR, // LKL_EUNATCH

  EFI_NOT_READY, // LKL_ENOCSI
  EFI_DEVICE_ERROR, // LKL_EL2HLT
  EFI_DEVICE_ERROR, // LKL_EBADE
  EFI_INVALID_PARAMETER, // LKL_EBADR
  EFI_OUT_OF_RESOURCES, // LKL_EXFULL
  EFI_NOT_FOUND, // LKL_ENOANO
  EFI_INVALID_PARAMETER, // LKL_EBADRQC
  EFI_INVALID_PARAMETER, // LKL_EBADSLT
  EFI_UNSUPPORTED, // LKL_EDEADLOCK
  EFI_UNSUPPORTED, // LKL_EBFONT

  EFI_INVALID_PARAMETER, // LKL_ENOSTR
  EFI_NOT_READY, // LKL_ENODATA
  EFI_TIMEOUT, // LKL_ETIME
  EFI_OUT_OF_RESOURCES, // LKL_ENOSR
  EFI_NOT_FOUND, // LKL_ENONET
  EFI_NOT_FOUND, // LKL_ENOPKG
  EFI_NOT_FOUND, // LKL_EREMOTE
  EFI_DEVICE_ERROR, // LKL_ENOLINK
  EFI_DEVICE_ERROR, // LKL_EADV
  EFI_DEVICE_ERROR, // LKL_ESRMNT

  EFI_DEVICE_ERROR, // LKL_ECOMM
  EFI_PROTOCOL_ERROR, // LKL_EPROTO
  EFI_UNSUPPORTED, // LKL_EMULTIHOP
  EFI_DEVICE_ERROR, // LKL_EDOTDOT
  EFI_DEVICE_ERROR, // LKL_EBADMSG
  EFI_OUT_OF_RESOURCES, // LKL_EOVERFLOW
  EFI_DEVICE_ERROR, // LKL_ENOTUNIQ
  EFI_DEVICE_ERROR, // LKL_EBADFD
  EFI_DEVICE_ERROR, // LKL_EREMCHG
  EFI_NOT_FOUND, // LKL_ELIBACC

  EFI_LOAD_ERROR, // LKL_ELIBBAD
  EFI_LOAD_ERROR, // LKL_ELIBSCN
  EFI_LOAD_ERROR, // LKL_ELIBMAX
  EFI_LOAD_ERROR, // LKL_ELIBEXEC
  EFI_LOAD_ERROR, // LKL_EILSEQ
  EFI_DEVICE_ERROR, // LKL_ERESTART
  EFI_DEVICE_ERROR, // LKL_ESTRPIPE
  EFI_OUT_OF_RESOURCES, // LKL_EUSERS
  EFI_INVALID_PARAMETER, // LKL_ENOTSOCK
  EFI_INVALID_PARAMETER, // LKL_EDESTADDRREQ

  EFI_OUT_OF_RESOURCES, // LKL_EMSGSIZE
  EFI_INVALID_PARAMETER, // LKL_EPROTOTYPE
  EFI_NOT_FOUND, // LKL_ENOPROTOOPT
  EFI_UNSUPPORTED, // LKL_EPROTONOSUPPORT
  EFI_UNSUPPORTED, // LKL_ESOCKTNOSUPPORT
  EFI_UNSUPPORTED, // LKL_EOPNOTSUPP
  EFI_UNSUPPORTED, // LKL_EPFNOSUPPORT
  EFI_UNSUPPORTED, // LKL_EAFNOSUPPORT
  EFI_NOT_READY, // LKL_EADDRINUSE
  EFI_DEVICE_ERROR, // LKL_EADDRNOTAVAIL

  EFI_DEVICE_ERROR, // LKL_ENETDOWN
  EFI_DEVICE_ERROR, // LKL_ENETUNREACH
  EFI_DEVICE_ERROR, // LKL_ENETRESET
  EFI_ABORTED, // LKL_ECONNABORTED
  EFI_DEVICE_ERROR, // LKL_ECONNRESET
  EFI_OUT_OF_RESOURCES, // LKL_ENOBUFS
  EFI_DEVICE_ERROR, // LKL_EISCONN
  EFI_DEVICE_ERROR, // LKL_ENOTCONN
  EFI_DEVICE_ERROR, // LKL_ESHUTDOWN
  EFI_DEVICE_ERROR, // LKL_ETOOMANYREFS

  EFI_TIMEOUT, // LKL_ETIMEDOUT
  EFI_ACCESS_DENIED, // LKL_ECONNREFUSED
  EFI_DEVICE_ERROR, // LKL_EHOSTDOWN
  EFI_DEVICE_ERROR, // LKL_EHOSTUNREACH
  EFI_NOT_READY, // LKL_EALREADY
  EFI_NOT_READY, // LKL_EINPROGRESS
  EFI_INVALID_PARAMETER, // LKL_ESTALE
  EFI_DEVICE_ERROR, // LKL_EUCLEAN
  EFI_UNSUPPORTED, // LKL_ENOTNAM
  EFI_OUT_OF_RESOURCES, // LKL_ENAVAIL

  EFI_INVALID_PARAMETER, // LKL_EISNAM
  EFI_DEVICE_ERROR, // LKL_EREMOTEIO
  EFI_VOLUME_FULL, // LKL_EDQUOT
  EFI_NO_MEDIA, // LKL_ENOMEDIUM
  EFI_INVALID_PARAMETER, // LKL_EMEDIUMTYPE
  EFI_ABORTED, // LKL_ECANCELED
  EFI_NOT_FOUND, // LKL_ENOKEY
  EFI_DEVICE_ERROR, // LKL_EKEYEXPIRED
  EFI_DEVICE_ERROR, // LKL_EKEYREVOKED
  EFI_DEVICE_ERROR, // LKL_EKEYREJECTED

  EFI_INVALID_PARAMETER, // LKL_EOWNERDEAD
  EFI_DEVICE_ERROR, // LKL_ENOTRECOVERABLE
  EFI_INVALID_PARAMETER, // LKL_ERFKILL
  EFI_DEVICE_ERROR, // LKL_EHWPOISON
};

EFI_STATUS
LKLError2EfiError (
  INTN Error
)
{
  if (Error < 0)
    Error = -Error;

  if ((UINTN)Error >= sizeof(LKLErrorStatus) / sizeof(EFI_STATUS))
    return MAX_UINTN;

  return LKLErrorStatus[Error];
}

VOID
EFIAPI
UnicodeToLower (
  IN EFI_STRING  UnicodeString
  )
{
  EFI_STRING  String;

  ASSERT (UnicodeString != NULL);

  //
  // Convert all hex digits in range [A-F] in the configuration header to [a-f]
  //
  for (String = UnicodeString; *String != L'\0'; String++) {
    if (*String >= L'A' && *String <= L'F') {
      *String = (CHAR16) (*String - L'A' + L'a');
    }
  }
}

EFI_STATUS
GetFileFromAnyPartition (
  IN  CONST CHAR16                *Path,
  OUT EFI_FILE_PROTOCOL           **NewHandle
  )
{
  EFI_STATUS                Status;
  UINTN                     HandleCount;
  EFI_HANDLE                *HandleBuffer;
  UINTN                     Index;
  VOID                      *Instance;
  BOOLEAN                   Found = FALSE;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   *Volume;
  EFI_FILE_PROTOCOL                 *Root;

  //
  // Start to check all the PciIo to find all possible device
  //
  HandleCount = 0;
  HandleBuffer = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], &gEfiSimpleFileSystemProtocolGuid, &Instance);
    if (EFI_ERROR (Status)) {
      continue;
    }

    // open SFS protocol
    Status = gBS->HandleProtocol (
                HandleBuffer[Index],
                &gEfiSimpleFileSystemProtocolGuid,
                (VOID **)&Volume
                );
    if (EFI_ERROR (Status)) {
      continue;
    }

    // open root directory
    Status = Volume->OpenVolume (
                   Volume,
                   &Root
                   );
    if (EFI_ERROR (Status)) {
      continue;
    }

    // open requested file
    Status = Root->Open (
                   Root,
                   NewHandle,
                   (CHAR16*)Path,
                   EFI_FILE_MODE_READ,
                   0
                   );
    FileHandleClose(Root);
    if (!EFI_ERROR(Status)) {
      Found = TRUE;
      break;
    }
  }

  gBS->FreePool (HandleBuffer);

  if (Found)
    return EFI_SUCCESS;
  else
    return EFI_NOT_FOUND;
}
