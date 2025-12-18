# Guest Kernel

This folder contains the build scripts and configuration for building a TDX-enabled guest kernel.

## Building the Kernel

To build the kernel, execute the build script from the `guest-kernel` directory:

```bash
cd tdx
./build.sh
```

The script will:
1. Clone the Ubuntu HWE 6.17 kernel source (with RTMR extend built-in)
2. Apply the TDX configuration
3. Enable required kernel options (TDX guest, VSOCK, netfilter, dm-crypt, etc.)
4. Compile the kernel

## Kernel Image Location

After a successful build, the kernel image can be found at:

```
linux/arch/x86/boot/bzImage
```

## Make the kernel image exploitable by Conker

```
cp linux/arch/x86/boot/bzImage /opt/cvm/kernel
cd /opt/cvm/kernel
mv bzImage bzImage-hwe-6.17-next
```