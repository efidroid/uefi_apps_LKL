/** @file
  Unicode Collation Support component that hides the trivial difference of Unicode Collation
  and Unicode collation 2 Protocol.

  Copyright (c) 2007 - 2013, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials are licensed and made available
under the terms and conditions of the BSD License which accompanies this
distribution. The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "LKL.h"

EFI_UNICODE_COLLATION_PROTOCOL  *mUnicodeCollationInterface = NULL;

/**
  Worker function to initialize Unicode Collation support.

  It tries to locate Unicode Collation (2) protocol and matches it with current
  platform language code.

  @param  AgentHandle          The handle used to open Unicode Collation (2) protocol.
  @param  ProtocolGuid         The pointer to Unicode Collation (2) protocol GUID.
  @param  VariableName         The name of the RFC 4646 or ISO 639-2 language variable.
  @param  DefaultLanguage      The default language in case the RFC 4646 or ISO 639-2 language is absent.

  @retval EFI_SUCCESS          The Unicode Collation (2) protocol has been successfully located.
  @retval Others               The Unicode Collation (2) protocol has not been located.

**/
EFI_STATUS
InitializeUnicodeCollationSupportWorker (
  IN EFI_HANDLE         AgentHandle,
  IN EFI_GUID           *ProtocolGuid,
  IN CONST CHAR16       *VariableName,
  IN CONST CHAR8        *DefaultLanguage
  )
{
  EFI_STATUS                      ReturnStatus;
  EFI_STATUS                      Status;
  UINTN                           NumHandles;
  UINTN                           Index;
  EFI_HANDLE                      *Handles;
  EFI_UNICODE_COLLATION_PROTOCOL  *Uci;
  BOOLEAN                         Iso639Language;
  CHAR8                           *Language;
  CHAR8                           *BestLanguage;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  ProtocolGuid,
                  NULL,
                  &NumHandles,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Iso639Language = (BOOLEAN) (ProtocolGuid == &gEfiUnicodeCollationProtocolGuid);
  GetEfiGlobalVariable2 (VariableName, (VOID**) &Language, NULL);

  ReturnStatus = EFI_UNSUPPORTED;
  for (Index = 0; Index < NumHandles; Index++) {
    //
    // Open Unicode Collation Protocol
    //
    Status = gBS->OpenProtocol (
                    Handles[Index],
                    ProtocolGuid,
                    (VOID **) &Uci,
                    AgentHandle,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    //
    // Find the best matching matching language from the supported languages
    // of Unicode Collation (2) protocol. 
    //
    BestLanguage = GetBestLanguage (
                     Uci->SupportedLanguages,
                     Iso639Language,
                     (Language == NULL) ? "" : Language,
                     DefaultLanguage,
                     NULL
                     );
    if (BestLanguage != NULL) {
      FreePool (BestLanguage);
      mUnicodeCollationInterface = Uci;
      ReturnStatus = EFI_SUCCESS;
      break;
    }
  }

  if (Language != NULL) {
    FreePool (Language);
  }

  FreePool (Handles);

  return ReturnStatus;
}

/**
  Initialize Unicode Collation support.

  It tries to locate Unicode Collation 2 protocol and matches it with current
  platform language code. If for any reason the first attempt fails, it then tries to
  use Unicode Collation Protocol.

  @param  AgentHandle          The handle used to open Unicode Collation (2) protocol.

  @retval EFI_SUCCESS          The Unicode Collation (2) protocol has been successfully located.
  @retval Others               The Unicode Collation (2) protocol has not been located.

**/
EFI_STATUS
InitializeUnicodeCollationSupport (
  IN EFI_HANDLE    AgentHandle
  )
{

  EFI_STATUS       Status;

  Status = EFI_UNSUPPORTED;

  //
  // First try to use RFC 4646 Unicode Collation 2 Protocol.
  //
  Status = InitializeUnicodeCollationSupportWorker (
             AgentHandle,
             &gEfiUnicodeCollation2ProtocolGuid,
             L"PlatformLang",
             (CONST CHAR8 *) PcdGetPtr (PcdUefiVariableDefaultPlatformLang)
             );
  //
  // If the attempt to use Unicode Collation 2 Protocol fails, then we fall back
  // on the ISO 639-2 Unicode Collation Protocol.
  //
  if (EFI_ERROR (Status)) {
    Status = InitializeUnicodeCollationSupportWorker (
               AgentHandle,
               &gEfiUnicodeCollationProtocolGuid,
               L"Lang",
               (CONST CHAR8 *) PcdGetPtr (PcdUefiVariableDefaultLang)
               );
  }

  return Status;
}
