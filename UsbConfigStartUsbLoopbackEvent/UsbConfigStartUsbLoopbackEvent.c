#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Uefi.h>

EFI_STATUS
EFIAPI
UsbConfigStartUsbLoopbackEventEntryPoint(IN EFI_HANDLE ImageHandle,
                                         IN EFI_SYSTEM_TABLE *SystemTable) {
  // 0x95808d98 -- UsbConfigStartUsbLoopbackEvent handle
  // extracted from BS_Code memory area
  EFI_HANDLE *Event = (EFI_HANDLE *)(0x95808d98);
  gBS->SignalEvent(Event);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
UsbConfigStartUsbLoopbackEventUnload(IN EFI_HANDLE ImageHandle) {
  return EFI_SUCCESS;
}
