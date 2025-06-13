# Conker CVM Base Image

> ⚠️ **Disclaimer**  
> This project is in a **prototype/alpha** stage and has **not been audited**.  
> It is intended for **experimentation only** and **must not be used in production environments**.  
> Use at your own risk.

## Overview

This repository provides the base image and supporting tools for running a CVM with proper assistant tools that facilitate measurement for instance, such as [**Conker**](https://github.com/iExecBlockchainComputing/conker), a confidential container engine based on **Intel TDX**. It includes:

- A base image for Conker CVMs (Confidential Virtual Machines)
- Build scripts and configuration files
- CVM assistants such as the **Secret Provider Agent** for managing secrets in TDX-based confidential environments

---

## 🛠 Prerequisites

Building:
- Docker for image building
- Development tools: `make`, `bash`, `gcc`, etc.

Running:
- Host system with **Intel TDX** enabled in BIOS and Linux kernel support
- QEMU with TDX and KVM support (version >= 9.0.2 recommended)

---

## 🚀 Building the CVM Base Image

To build the confidential VM base image:

```bash
cd base-image
bash release.sh buildimage
```

This will compile all necessary components (e.g., the secret provider agent) and produce a Docker image named ```cvm-base```

You may customize the image by modifying the Dockerfile or the files/ directory.

⸻

⚙️ Integration

This base image is intended to be used as a foundation for building conker as in [the conker repo](https://github.com/iExecBlockchainComputing/xTDX) and running it via QEMU with TDX. 

Note: Make sure you have appropriate permissions (KVM group), and that TDX is enabled and detected by the kernel.