# Copilot Setup Steps Documentation

This document explains the `.github/copilot-setup-steps.yml` file and how it enhances the GitHub Copilot Coding Agent experience for aubio-ledfx development.

## Overview

The `copilot-setup-steps.yml` file automatically configures a complete development environment when GitHub Copilot Coding Agent starts working on this repository. This ensures that Copilot has all the necessary tools and dependencies installed to build, test, and develop aubio-ledfx efficiently.

## What Gets Installed

### 1. System Build Tools
- **Compilers**: gcc, g++ (C99-compliant for aubio)
- **Build systems**: ninja-build (used by Meson)
- **Build utilities**: pkg-config, git, curl, wget, zip, unzip, tar
- **Assemblers**: nasm (for FFmpeg assembly optimizations in vcpkg)
- **Development libraries**: libfftw3-dev, libsndfile1-dev, libsamplerate0-dev, libjack-dev

### 2. Python Build Environment
- **Meson**: >=1.9.0 (primary build system)
- **meson-python**: PEP 517 build backend for Python packages
- **ninja**: Build tool (also installed as Python package)
- **NumPy**: >=1.26.4 (required for aubio Python bindings)
- **pip, setuptools, wheel**: Updated to latest versions

### 3. Testing Tools
- **pytest**: Python test runner
- **pytest-cov**: Code coverage for tests
- **build**: Python package build tool

### 4. Security Testing Tools
- **libasan6**: AddressSanitizer runtime library
- **libubsan1**: UndefinedBehaviorSanitizer runtime library
- **valgrind**: Memory error detection tool

These tools enable running the security test suite defined in `.github/workflows/sanitizers.yml`.

### 5. vcpkg - Dependency Management
- Clones and bootstraps vcpkg to `$HOME/vcpkg`
- Provides cross-platform C/C++ package management
- Automatically installs dependencies from `vcpkg.json`:
  - pkgconf, libsndfile, libsamplerate, fftw3
  - Platform-specific: rubberband, ffmpeg (macOS/Windows only)
  - Audio codecs: mpg123, mp3lame, opus, libogg, libvorbis, libflac (Linux only)

### 6. Optional Development Tools
- **uv**: Modern, fast Python package manager (faster alternative to pip)

### 7. Environment Configuration
Sets up the following environment variables in `.bashrc`:
- `VCPKG_ROOT`: Points to vcpkg installation
- `PATH`: Includes vcpkg executable
- `PKG_CONFIG_PATH`: Includes system and vcpkg pkg-config paths
- `CMAKE_PREFIX_PATH`: Includes vcpkg installed packages

### 8. Project Setup
- Installs aubio-ledfx in editable mode (`pip install -e .`)
- This allows testing changes without reinstalling

### 9. Verification
- Displays versions of all installed tools
- Shows vcpkg status
- Lists installed Python packages
- Provides usage instructions

## How It Works

When GitHub Copilot Coding Agent starts working on this repository, it:

1. Reads `.github/copilot-setup-steps.yml`
2. Executes each `- run:` step in order
3. Sets up a complete development environment
4. Reports the installation status

This happens automatically before Copilot begins coding, ensuring all tools are available.

## Usage Examples

After the setup completes, Copilot (and developers) can:

### Build the C Library
```bash
meson setup builddir
meson compile -C builddir
```

### Build with Tests Enabled
```bash
meson setup builddir -Dtests=true
meson compile -C builddir
meson test -C builddir
```

### Build with Security Sanitizers
```bash
meson setup builddir -Db_sanitize=address,undefined -Dtests=true
meson compile -C builddir
meson test -C builddir
```

### Build Python Wheel
```bash
# Using uv (faster)
uv build --wheel

# Using pip
pip wheel . --no-deps
```

### Run Tests
```bash
# C tests
meson test -C builddir --print-errorlogs

# Python tests
pytest python/tests/
```

### Install vcpkg Dependencies
```bash
# Dependencies are auto-installed by Meson, but can be manually triggered
vcpkg install --triplet=x64-linux
```

## Benefits for Copilot

1. **Zero Setup Time**: Copilot doesn't need to figure out what tools to install
2. **Consistent Environment**: Same setup every time, reducing errors
3. **Complete Toolchain**: All build, test, and security tools available
4. **Dependency Management**: vcpkg handles all C/C++ library dependencies
5. **Security Testing**: Can run sanitizer tests immediately
6. **Fast Iteration**: uv enables faster Python package builds

## Maintenance

### Adding New Tools

To add a new tool to the setup:

1. Edit `.github/copilot-setup-steps.yml`
2. Add a new `- run:` step with installation commands
3. Add a descriptive `name:` field
4. Update the verification step to check the new tool
5. Update this documentation

### Adding New Dependencies

For Python dependencies:
- Add to the appropriate pip install command in step 2 or 3

For C/C++ dependencies:
- Add to `vcpkg.json` in the repository root
- They will be auto-installed by step 8

For system packages:
- Add to the apt-get install command in step 1

## Platform Notes

This setup file is designed for **Linux-based environments** (Ubuntu). GitHub Copilot Coding Agent typically runs in Linux containers.

For local development on other platforms:
- **macOS**: Use Homebrew for system packages, vcpkg still works
- **Windows**: Use vcpkg for dependencies, MSVC or MinGW-w64 for compilation

See the main README.md for platform-specific instructions.

## Related Files

- **`.github/copilot-instructions.md`**: Custom instructions for Copilot's behavior
- **`vcpkg.json`**: Declares C/C++ dependencies
- **`pyproject.toml`**: Python package configuration
- **`meson.build`**: Main build system configuration
- **`meson_options.txt`**: Build options and feature toggles
- **`.github/workflows/build.yml`**: CI/CD build workflow
- **`.github/workflows/sanitizers.yml`**: Security testing workflow

## Troubleshooting

### Setup Fails

If a setup step fails, check:
1. Internet connectivity (vcpkg needs to download packages)
2. Disk space (vcpkg packages can be large)
3. System compatibility (Ubuntu/Debian Linux expected)

### Tools Not Found After Setup

The environment variables are set in `.bashrc`, which may not be loaded in all contexts. To manually source:
```bash
source $HOME/.bashrc
```

Or export them for the current session:
```bash
export VCPKG_ROOT=$HOME/vcpkg
export PATH=$HOME/vcpkg:$PATH
```

### vcpkg Installation Fails

If vcpkg installation is skipped or fails, it can be manually installed:
```bash
git clone https://github.com/microsoft/vcpkg.git $HOME/vcpkg
cd $HOME/vcpkg && ./bootstrap-vcpkg.sh
export VCPKG_ROOT=$HOME/vcpkg
```

## References

- [GitHub Copilot Environment Setup Documentation](https://docs.github.com/en/copilot/customizing-copilot/adding-custom-instructions-for-github-copilot#creating-environment-setup-files-for-a-repository)
- [Meson Build System](https://mesonbuild.com/)
- [vcpkg Documentation](https://vcpkg.io/en/docs/README.html)
- [meson-python](https://meson-python.readthedocs.io/)
- [aubio-ledfx Build Documentation](../doc/building.rst)
- [aubio-ledfx vcpkg Integration](../doc/vcpkg_integration.md)
