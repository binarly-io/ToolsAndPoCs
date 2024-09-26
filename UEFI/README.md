# BINARLY Research Tools and PoCs (UEFI)

Build Rust tools:

```
cd {tool}
cargo build --release --target aarch64-unknown-uefi
```

Build `DumpMem`:

```
cp -r DumpMem {edk2}/ShellPkg/Application/
# add ShellPkg/Application/DumpMem/DumpMem.inf to ShellPkg.dsc under [Componenents]
. ./edksetup.sh
build -p ShellPkg/ShellPkg.dsc -b DEBUG -a AARCH64 -t GCC5
```

Dump UEFI firmware on Windows Dev Kit:

```
DumpMem.efi 0x9f000000 0x5d0000 uefi.bin
```
