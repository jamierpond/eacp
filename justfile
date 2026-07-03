set windows-shell := ["powershell.exe", "-NoLogo", "-NoProfile", "-Command"]

build_dir := "build"

# Windows: recipes that compile need the MSVC environment. When cl isn't on
# PATH already, locate the latest Visual Studio with vswhere and enter its
# dev shell before running the command. The machine architecture comes from
# the registry because the process env lies under x64 emulation on ARM64
# (PROCESSOR_ARCHITECTURE=AMD64), which would select the wrong dev shell.
devshell := if os() == "windows" { 'if (-not (Get-Command cl -ErrorAction SilentlyContinue)) { $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"; if (Test-Path $vswhere) { $vs = & $vswhere -latest -products * -property installationPath; $machine = (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Environment").PROCESSOR_ARCHITECTURE; $arch = if ($machine -eq "ARM64") { "arm64" } else { "amd64" }; & "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch $arch -HostArch $arch -SkipAutomaticLocation *> $null } }; ' } else { "" }

# Configure with unity builds off so compile_commands.json stays accurate.
# Extra args go to cmake: `just configure --fresh -DFOOBAR=1`
configure *args:
    {{ devshell }}cmake -G Ninja -B {{ build_dir }} -DCMAKE_BUILD_TYPE=Debug -DEACP_UNITY_BUILD=OFF {{ args }}

# Build everything, or one target: `just build Teapot`
build target="all" *args:
    {{ devshell }}cmake --build {{ build_dir }} --target {{ target }} {{ args }}

# Build and launch an app: `just run Browser`
[windows]
run target *args: (build target)
    $exe = Get-ChildItem -Path {{ build_dir }}\Apps -Recurse -Filter '{{ target }}.exe' | Select-Object -First 1; if ($null -eq $exe) { Write-Error "no built executable found for '{{ target }}'"; exit 1 }; & $exe.FullName {{ args }} | Out-Default

[unix]
run target *args: (build target)
    #!/usr/bin/env sh
    set -e
    exe=$(find {{ build_dir }}/Apps -type f \
        \( -path "*/{{ target }}.app/Contents/MacOS/{{ target }}" \
           -o -name "{{ target }}" \) \
        | head -n 1)
    if [ -z "$exe" ]; then
        echo "no built executable found for '{{ target }}'" >&2
        exit 1
    fi
    "$exe" {{ args }}

# Build and run the full test suite
test: build
    {{ devshell }}ctest --test-dir {{ build_dir }} --output-on-failure

clean:
    {{ devshell }}cmake --build {{ build_dir }} --target clean
