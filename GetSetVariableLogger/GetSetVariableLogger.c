#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Uefi.h>

EFI_GET_VARIABLE gGetVariableOld;
EFI_SET_VARIABLE gSetVariableOld;

static CONST CHAR8 mHex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                             '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

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

VOID EFIAPI GetVariableLogDetails(EFI_STATUS Status, CHAR16 *VariableName,
                                  EFI_GUID *VendorGuid, UINTN DataSize) {
  DebugPrint(DEBUG_INFO,
             "[gRT->GetVariable()] VariableName: %s, VendorGuid: %g, DataSize: "
             "0x%llx, Status: %r\n",
             VariableName, VendorGuid, DataSize, Status);
}

VOID EFIAPI SetVariableLogDetails(EFI_STATUS Status, CHAR16 *VariableName,
                                  EFI_GUID *VendorGuid, UINTN DataSize) {
  DebugPrint(DEBUG_INFO,
             "[gRT->SetVariable()] Name: %s, VendorGuid: %g, DataSize: 0x%llx, "
             "Status: %r\n",
             VariableName, VendorGuid, DataSize, Status);
}

EFI_STATUS
EFIAPI
GetVariableNew(IN CHAR16 *VariableName, IN EFI_GUID *VendorGuid,
               OUT UINT32 *Attributes OPTIONAL, IN OUT UINTN *DataSize,
               OUT VOID *Data) {
  EFI_STATUS Status =
      gGetVariableOld(VariableName, VendorGuid, Attributes, DataSize, Data);
  GetVariableLogDetails(Status, VariableName, VendorGuid, *DataSize);
  if (Data && !StrCmp(VariableName, L"UsbConfigSecondaryPort")) {
    HexDump(2, (UINTN)Data - 256, 512, Data - 256);
  }
  return Status;
}

EFI_STATUS
EFIAPI
SetVariableNew(IN CHAR16 *VariableName, IN EFI_GUID *VendorGuid,
               IN UINT32 Attributes, IN UINTN DataSize, IN VOID *Data) {
  EFI_STATUS Status =
      gSetVariableOld(VariableName, VendorGuid, Attributes, DataSize, Data);
  SetVariableLogDetails(Status, VariableName, VendorGuid, DataSize);
  return Status;
}

VOID EFIAPI GetSetVariableHook() {
  // Hook GetVariable and SetVariable
  gGetVariableOld = (EFI_GET_VARIABLE)gRT->GetVariable;
  gSetVariableOld = (EFI_SET_VARIABLE)gRT->SetVariable;
  DebugPrint(DEBUG_INFO, "gRT->GetVariable() address = %p\n", gGetVariableOld);
  DebugPrint(DEBUG_INFO, "gRT->SetVariable() address = %p\n", gSetVariableOld);
  gRT->GetVariable = (EFI_GET_VARIABLE)GetVariableNew;
  gRT->SetVariable = (EFI_SET_VARIABLE)SetVariableNew;
}

EFI_STATUS
EFIAPI
GetSetVariableLoggerEntryPoint(IN EFI_HANDLE ImageHandle,
                               IN EFI_SYSTEM_TABLE *SystemTable) {
  DebugPrint(DEBUG_INFO, "GetSetVariableLogger driver loaded\n");

  GetSetVariableHook();
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
GetSetVariableLoggerUnload(IN EFI_HANDLE ImageHandle) { return EFI_SUCCESS; }
