[Defines]
  DSC_SPECIFICATION              = 1.28
  PLATFORM_NAME                  = DxeSmmHook
  PLATFORM_GUID                  = 0b79e762-783e-4448-b8e1-ca9659e9371a
  PLATFORM_VERSION               = 1.00
  OUTPUT_DIRECTORY               = Build/DxeSmmHook
  SUPPORTED_ARCHITECTURES        = X64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT

[Components]
  DxeSmmHook/DxeSmmHookSrc/DxeSmmHook.inf

[LibraryClasses]
  UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  !if $(TARGET) == RELEASE
    DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  !else
    !ifdef $(DEBUG_ON_SERIAL_PORT)
      IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsicSev.inf
      SerialPortLib|PcAtChipsetPkg/Library/SerialIoLib/SerialIoLib.inf
      DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
    !else
      DebugLib|MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
    !endif
  !endif

[PcdsFixedAtBuild]
  # Define DEBUG_ERROR | DEBUG_VERBOSE | DEBUG_INFO | DEBUG_WARN to enable
  # logging at those levels. Also, define DEBUG_PROPERTY_ASSERT_DEADLOOP_ENABLED
  # and such. Assertion failure will call CpuDeadLoop.
  # https://github.com/tianocore/tianocore.github.io/wiki/EDK-II-Debugging
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80400042
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x2f
