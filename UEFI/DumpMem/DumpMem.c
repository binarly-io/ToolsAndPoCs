#include <Library/ShellLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/ShellParameters.h>

#define BLOCK_SIZE 0x10000

static CONST CHAR8 mHex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                             '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

static UINTN Argc;
static CHAR16 **Argv;

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
    Print(L"%*a%08X: %-48a *%a*\r\n", Indent, "", Offset, Val, Str);

    Data += Size;
    Offset += Size;
    DataSize -= Size;
  }
}

static EFI_STATUS GetArg(VOID) {
  EFI_STATUS Status;
  EFI_SHELL_PARAMETERS_PROTOCOL *ShellParameters;

  Status = gBS->HandleProtocol(gImageHandle, &gEfiShellParametersProtocolGuid,
                               (VOID **)&ShellParameters);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Argc = ShellParameters->Argc;
  Argv = ShellParameters->Argv;
  return EFI_SUCCESS;
}

static VOID ShowHelp() {
  Print(L"Dump memory by address.\n");
  Print(L"\n");
  Print(L"DumpMem [address] [size] [filename]\n");
  Print(L"  address example: 0xff000000\n");
  Print(L"  size example: 0x1000\n");
  Print(L"  filename example: dump.bin\n");
  Print(L"\n");
  Print(
      L"If filename is not specified memory is printed as hexadecimal dump.\n");
}

static EFI_STATUS WriteByBlocks(SHELL_FILE_HANDLE DumpFileHandle,
                                UINT64 Address, UINT64 DataSize,
                                UINT64 BlockSize) {
  EFI_STATUS Status;
  UINTN WrittenSize = 0;
  UINTN BlockCount = 0;
  UINTN LbSize = 0;
  UINTN i = 0;

  if (!BlockSize) {
    Print(L"Block count is invalid\n");
    return EFI_INVALID_PARAMETER;
  }
  BlockCount = DataSize / BlockSize;
  LbSize = DataSize % BlockSize;

  for (i = 0; i < BlockCount; i++) {
    Print(L"Current offset: 0x%llx\n", Address + WrittenSize);
    Status = ShellWriteFile(DumpFileHandle, &BlockSize,
                            (VOID *)(Address + WrittenSize));
    if (EFI_ERROR(Status)) {
      Print(L"Failed to write block %d\n", i);
      return Status;
    }
    WrittenSize += BlockSize;
  }

  if (LbSize > 0) {
    Print(L"Current offset: 0x%llx\n", Address + WrittenSize);
    Status = ShellWriteFile(DumpFileHandle, &LbSize,
                            (VOID *)(Address + WrittenSize));
    if (EFI_ERROR(Status)) {
      Print(L"Failed to write remaining data\n");
      return Status;
    }
  }

  return Status;
}

static EFI_STATUS DumpMemToFile(UINT64 Address, UINT64 DataSize,
                                CHAR16 *Filename) {
  EFI_STATUS Status;
  SHELL_FILE_HANDLE DumpFileHandle;

  Status = ShellOpenFileByName(
      Filename, &DumpFileHandle,
      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  if (EFI_ERROR(Status)) {
    Print(L"Could not open file %s\n", Filename);
    return Status;
  }

  Status = WriteByBlocks(DumpFileHandle, Address, DataSize, BLOCK_SIZE);

  if (EFI_ERROR(Status)) {
    Print(L"Could not write file %s\n", Filename);
  } else {
    Print(L"Done\n");
  }
  ShellCloseFile(&DumpFileHandle);
  return Status;
}

EFI_STATUS
EFIAPI
UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
  EFI_STATUS Status = EFI_SUCCESS;

  CHAR16 *AddressStr;
  CHAR16 *SizeStr;
  CHAR16 *Filename;
  UINT64 Address;
  UINT64 DataSize;

  //
  // get the command line arguments
  //
  Status = GetArg();
  if (EFI_ERROR(Status)) {
    Print(L"DumpMem: Error. The input parameters are not recognized.\n");
    Status = EFI_INVALID_PARAMETER;
    return Status;
  }

  if (Argc < 3 || Argc > 4) {
    ShowHelp();
    return Status;
  }
  AddressStr = Argv[1];
  SizeStr = Argv[2];
  if EFI_ERROR (ShellConvertStringToUint64(AddressStr, &Address, TRUE, FALSE)) {
    Print(L"Can not convert address\n");
    return Status;
  }
  if EFI_ERROR (ShellConvertStringToUint64(SizeStr, &DataSize, TRUE, FALSE)) {
    Print(L"Can not convert size\n");
    return Status;
  }

  if (Argc == 3) {
    Print(L"Address: 0x%llx, size: 0x%llx\n", Address, DataSize);
    HexDump(2, (UINTN)Address, (UINTN)DataSize, (VOID *)Address);
    return Status;
  }
  if (Argc == 4) {
    Filename = Argv[3];
    Print(L"Address: 0x%llx, size: 0x%llx, filename: %s\n", Address, DataSize,
          Filename);
    DumpMemToFile(Address, DataSize, Filename);
    return Status;
  }

  return Status;
}
