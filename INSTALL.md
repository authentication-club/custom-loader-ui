## Loader UI: Dependency & Build Guide

These steps cover everything a user needs to compile **`loader_ui`** by itself.  
We rely on [vcpkg](https://github.com/microsoft/vcpkg) for third‑party libraries and build with **Visual Studio 2022** on Windows 10/11.

---

### 1. Visual Studio prerequisites

Install VS 2022 (Community is fine) with:

- **Desktop development with C++** workload  
- Windows 10/11 SDK (10.0.19041 or newer)  
- C++ CMake tools for Windows (optional, but helps when vcpkg builds ports)

After installation, open one *x64 Native Tools Command Prompt for VS 2022* to run the commands below.

---

### 2. Install vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg C:\tools\vcpkg
C:\tools\vcpkg\bootstrap-vcpkg.bat
```

Add the helper to Visual Studio (run once):

```powershell
C:\tools\vcpkg\vcpkg integrate install
```

We will assume `C:\tools\vcpkg` as the vcpkg root; adjust the paths if you chose a different location.

---

### 3. Install required libraries

Loader UI links statically against a handful of multimedia dependencies.  
Install them for the `x64-windows-static-md` triplet:

```powershell
C:\tools\vcpkg\vcpkg install `
    ffmpeg `
    freetype `
    libpng `
    brotli `
    bzip2 `
    zlib `
    portaudio `
    --triplet x64-windows-static-md
```

> *Tip:* The command may take a while the first time because vcpkg builds FFmpeg.  
> If you only need Release builds, you can add `--only-downloads` once and build later, but the command above keeps things simple.

---

### 4. Configure the project include/library paths

Visual Studio needs to know where vcpkg installed the headers and libraries.  
The current project expects them at `C:\tools\vcpkg\installed\x64-windows-static-md\{include,lib}`.

If you used another path, either:

1. Modify the project’s **VC++ Directories**  
   (`Project ▸ Properties ▸ VC++ Directories ▸ Include/Library Directories`)

   ```
   $(VCPKG_ROOT)\installed\x64-windows-static-md\include
   $(VCPKG_ROOT)\installed\x64-windows-static-md\lib
   ```

   where `VCPKG_ROOT` is an environment variable you define (Control Panel ▸ System ▸ Advanced ▸ Environment Variables).

2. Or edit `loader_ui/loader_ui.vcxproj` and replace the hard-coded path with your location.

Either route works—just make sure the include/lib folders point to the vcpkg install.

---

### 5. Build loader_ui

1. Open `loader_ui/loader_ui.vcxproj` (or the solution if you have one) in Visual Studio.  
2. Choose `Release | x64` as the configuration (this is the shipping build).  
3. Build ▸ **Build loader_ui**.

The project produces:

- `loader_ui\loader_ui\x64\Release\loader_ui.dll` — runtime DLL  
- `loader_ui\loader_ui\x64\Release\loader_ui.lib` — import library

Any DirectX runtimes (D3D11, D3DCompiler) and system libs (GDI+, MF, etc.) are provided by Windows itself.

---

### 6. Optional: keep vcpkg in sync

When Microsoft updates ports, refresh occasionally:

```powershell
cd C:\tools\vcpkg
git pull
.\vcpkg upgrade --no-dry-run --triplet x64-windows-static-md
```

This ensures FFmpeg, freetype, and the other dependencies stay patched.

---

### Quick reference

| Item                   | Command / Path                                                     |
| ---------------------- | ------------------------------------------------------------------ |
| vcpkg root             | `C:\tools\vcpkg` (example)                                         |
| Triplet                | `x64-windows-static-md`                                            |
| Required ports         | `ffmpeg freetype libpng brotli bzip2 zlib portaudio`               |
| Include directory      | `$(VCPKG_ROOT)\installed\x64-windows-static-md\include`            |
| Library directory      | `$(VCPKG_ROOT)\installed\x64-windows-static-md\lib`                |
| Build output (Release) | `loader_ui\loader_ui\x64\Release\loader_ui.dll` / `.lib`           |

Follow these steps and loader_ui should compile without any manual library chasing. Happy building!

