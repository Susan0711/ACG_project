# Windows / MSVC handoff — pending execution

No Windows build has been performed by the authoring agent. Requires Visual Studio 2022 Desktop development with C++, CMake, and a Windows developer PowerShell. No external libraries/assets need downloading.

From the repository root:

```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Release
ctest --test-dir build-msvc -C Release --output-on-failure
$Assets = (Resolve-Path .\assets).Path
Push-Location build-msvc
.\Release\path_tracer.exe --scene forest-v03 --resource-root $Assets --width 160 --height 90 --spp 8 --depth 10 --seed 19 --threads 1 --output forest-one.ppm
.\Release\path_tracer.exe --scene forest-v03 --resource-root $Assets --width 160 --height 90 --spp 8 --depth 10 --seed 19 --threads 4 --output forest-four.ppm
Get-FileHash forest-one.ppm, forest-four.ppm
.\Release\path_tracer.exe --scene forest-v03 --resource-root missing-assets --output should-not-exist.ppm
Pop-Location
```

Expect equal hashes for 1/4 threads on the same build; missing-assets must exit nonzero and name the attempted path. Existing output filenames are rejected: choose new filenames on reruns. CTest includes Cornell/motion/alpha thread invariance and worker-error handling. Also inspect the forest image for visible windows, water and geometry.

Alternatively run `./tools/validate_windows.ps1`, which configures/builds/tests and runs the above small checks from the build directory with a timestamped output directory. The script itself is pending Windows validation. Do not loosen machine execution policy solely for this task; use locally permitted script execution or the manual commands.

Record CPU, Windows version, MSVC/CMake versions, configuration, CTest output, exact render commands, timing lines, image hashes and visual findings in a new validation note. Cross-platform byte identity is not required. This is an independent verification opportunity, not an attribution of work already performed by a teammate. Further independent modules can include texture filtering, mesh/smooth-normal robustness, or a separately evaluated course optional feature after agreement; none is marked implemented on their behalf.


v0.4 adds a dependency-free RGB float PFM reader and MIS tests to CTest. Optional extra smoke (from repository root, choose fresh filenames):
```powershell
.\build-msvc\Release\path_tracer.exe --scene sampling --environment studio-hdr.pfm --resource-root assets --mode mis --width 96 --height 60 --spp 32 --threads 4 --output build-msvc/hdr-smoke.ppm --linear-output build-msvc/hdr-smoke.pfm
```
This new Windows command, like the existing script, is still pending execution.
