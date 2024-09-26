#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Uefi.h>

#define SEC_TO_MICRO(s) ((s)*1000000)

EFI_LOCATE_PROTOCOL gLocateProtocolOld;
UINTN *gSmramBase = (UINTN *)0xce000000;
UINTN gSig = (UINTN)0x34365f33534d4d53;
UINT8 gContinue = 0;

static CONST CHAR8 mHex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                             '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

// SMRAM header for demo
UINT8 gSmramContent[256] = {0};

static VOID HexDump(IN UINTN Indent, IN UINTN Offset, IN UINTN DataSize,
                    IN VOID *UserData) {
  UINT8 *Data;
  CHAR8 Val[50];
  CHAR8 Str[20];
  UINT8 TempByte;
  UINTN Size;
  UINTN Index;

  Data = UserData;
  while (DataSize != 0) {
    Size = 16;
    if (Size > DataSize) {
      Size = DataSize;
    }

    for (Index = 0; Index < Size; Index += 1) {
      TempByte = Data[Index];
      Val[Index * 3 + 0] = mHex[TempByte >> 4];
      Val[Index * 3 + 1] = mHex[TempByte & 0xF];
      Val[Index * 3 + 2] = (CHAR8)((Index == 7) ? '-' : ' ');
      Str[Index] = (CHAR8)((TempByte < ' ' || TempByte > 'z') ? '.' : TempByte);
    }

    Val[Index * 3] = 0;
    Str[Index] = 0;
    DebugPrint(DEBUG_INFO, "%*a%08X: %-48a *%a*\r\n", Indent, "", Offset, Val,
               Str);

    Data += Size;
    Offset += Size;
    DataSize -= Size;
  }
}

VOID EFIAPI CheckSmramAccess() {
  if (gContinue) {
    return;
  }

  UINTN Sig = *gSmramBase;
  if (Sig == gSig) {
    CopyMem(&gSmramContent, (UINT8 *)gSmramBase, 256);
  }

  VOID *EfiSimpleTextOutProtocolGuid = NULL;
  EFI_STATUS Status =
      gLocateProtocolOld(&gEfiSimpleTextOutProtocolGuid, NULL,
                         (VOID **)&EfiSimpleTextOutProtocolGuid);
  if (Status == EFI_SUCCESS) {
    gBS->Stall(SEC_TO_MICRO(5));
    DebugPrint(DEBUG_INFO, "Can read and write SMRAM:\n");
    HexDump(2, (UINTN)gSmramBase, 256, (UINT8 *)&gSmramContent);
    UINTN Sec = 3;
    DebugPrint(DEBUG_INFO, "\nContinue in ");
    while (Sec >= 2) {
      DebugPrint(DEBUG_INFO, "%d, ", Sec);
      gBS->Stall(SEC_TO_MICRO(1));
      --Sec;
    }
    DebugPrint(DEBUG_INFO, "1\n");
    gBS->Stall(SEC_TO_MICRO(1));
    gContinue = 1;
  }
}

EFI_STATUS
EFIAPI
LocateProtocolNew(IN EFI_GUID *Protocol, IN VOID *Registration OPTIONAL,
                  OUT VOID **Interface) {
  EFI_STATUS Status = gLocateProtocolOld(Protocol, Registration, Interface);
  CheckSmramAccess();
  return Status;
}

VOID EFIAPI LocateProtocolSetupHook() {
  // Hook LocateProtocol
  gLocateProtocolOld = (EFI_LOCATE_PROTOCOL)gBS->LocateProtocol;
  DebugPrint(DEBUG_INFO, "gBS->LocateProtocol() address = %p\n",
             gLocateProtocolOld);
  gBS->LocateProtocol = (EFI_LOCATE_PROTOCOL)LocateProtocolNew;
}

EFI_STATUS
EFIAPI
DxeSmmHookEntryPoint(IN EFI_HANDLE ImageHandle,
                     IN EFI_SYSTEM_TABLE *SystemTable) {
  DebugPrint(DEBUG_INFO, "DxeSmmHook driver loaded, gBS = %p\n", gBS);

  LocateProtocolSetupHook();

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
DxeSmmHookUnload(IN EFI_HANDLE ImageHandle) { return EFI_SUCCESS; }
