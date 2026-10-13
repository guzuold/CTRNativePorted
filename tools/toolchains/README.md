# Local Portable Toolchains

Compiler binaries live in subdirectories under `tools/toolchains/` and are not stored in the repository.

## llvm-19.1.1

- **Purpose**: Compile `ctrport/gpu/` (`ctr_gpu.dll`) with `clang-cl`. The reused shadPS4 renderer code requires Clang on Windows (matching `engine/shadPS4/documents/building-windows.md`) and `__attribute__((sysv_abi))` support while producing MSVC-compatible object files.
- **Official release**: LLVM `llvmorg-19.1.1`
  `https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.1/clang%2Bllvm-19.1.1-x86_64-pc-windows-msvc.tar.xz`
- **Archive**: `clang+llvm-19.1.1-x86_64-pc-windows-msvc.tar.xz` (844,457,100 bytes), SHA256:
  `621fc299fceb1bbdae927e355d1073034c9a1bbdda5a46a27e217c56af72f72a`
- **Setup**: run `powershell -File tools/toolchains/fetch_llvm.ps1` (also called by
  `tools/bootstrap.ps1`): it downloads the archive, checks size and SHA256, and extracts it.
  Manual alternative: extract into `tools/toolchains/llvm-19.1.1/`:
  ```powershell
  mkdir tools/toolchains/llvm-19.1.1
  tar -xf clang+llvm-19.1.1-x86_64-pc-windows-msvc.tar.xz -C tools/toolchains/llvm-19.1.1 --strip-components=1
  ```
- **Alternative**: Install the Visual Studio 2022 **C++ Clang Compiler for Windows** component or set `CTR_CLANG_CL` to your `clang-cl.exe` path.

