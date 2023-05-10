# Extended build guide

```
# Make a directory to hold the temporary build files
mkdir -p build

# Change directory to the build directory
cd build

# Configure the Hestia build using CMake
cmake \
    [options below] \
    /path/to/hestia/project

cd /path/to/hestia/project

# Build the Hestia project
make -C build 

# Install the Hestia executable
make -C build install
```

### Useful Options

#### Setting Compile/Link Flags
Option | Effect
------ | ------
`-DCMAKE_CXX_COMPILER=...` | Set the C++ compiler.
`-DCMAKE_CXX_FLAGS=...`    | Set the flags to pass to the C++ compiler. Overrides the default flags.
`-DCMAKE_BUILD_TYPE=...`   | Set to `Release` or `Debug` for release or debug builds. Sets a number of flags by default.

#### Enabling/Disabling Sections of Hestia
Option | Effect
------ | ------
`-DHESTIA_BUILD_TESTS=...`     | Set to `ON` to build Hestia tests and enable the `make test` target, or `OFF` to skip (Default `OFF`).
`-DHESTIA_BUILD_DOCUMENTATION=...`     | Set to `ON` to generate doxygen documentation (Default `OFF`).

#### Setting installation directories
Option | Effect
------ | ------
`-DCMAKE_INSTALL_PREFIX=...` | Set to the root install directory for the compiled libraries and programs.
`-DCMAKE_INSTALL_BINDIR=...` | Set the install directory for the `hestia` executable. Use a relative path to set the path relative to `${CMAKE_INSTALL_PREFIX}`. (Default `bin`).
`-DCMAKE_INSTALL_LIBDIR=...` | Set the install directory for `hestia` libraries. Use a relative path to set the path relative to `${CMAKE_INSTALL_PREFIX}`. (Default `lib`).

## Testing

Units test can be run with `make -C build test`, given `HESTIA_BUILD_TESTS` was set to `ON` during the build process. 


# Running Static Analysis

## Clang Format

### Dependencies

#### Mac

On Mac `brew` currently packages `clang-format` and `llvm` as version 16, which are newer than the CI version and have compatibility issues. If you have already installed that you can unlink it with `brew unlink clang-format llvm`. 

For us, version 14 works. You can get `llvm@14` with `brew`:

```bash
brew install llvm@14
```

If you want to use this as your default compiler add the following to your `.zshrc`

```bash
echo 'export PATH="/opt/homebrew/opt/llvm@14/bin:$PATH"' >> ~/.zshrc
echo 'export LDFLAGS="-L/opt/homebrew/opt/llvm@14/lib"' >> ~/.zshrc
echo 'export CPPFLAGS="-I/opt/homebrew/opt/llvm@14/include"' >> ~/.zshrc
```

### Run

The format script is `tools/run_format.sh`, which in turn runs `tools/format.sh`. In that file if you are on Mac you need to explicitly enter the path to `llvm@14` as the clang exe (something like: `/opt/homebrew/opt/llvm@14/bin/clang-format`). After that you can run:

```bash
tools/run_format.sh
```

which will update files with any needed changes.

## Clang Tidy

### Dependencies

#### Mac

It comes with `apple-clang`, but to install `llvm-14` which we use for CI, run 

```bash
brew install llvm@14
```

If you want to use this as your default compiler add the following to your `.zshrc`

```bash
echo 'export PATH="/opt/homebrew/opt/llvm@14/bin:$PATH"' >> ~/.zshrc
echo 'export LDFLAGS="-L/opt/homebrew/opt/llvm@14/lib"' >> ~/.zshrc
echo 'export CPPFLAGS="-I/opt/homebrew/opt/llvm@14/include"' >> ~/.zshrc
```

While not monitored, a more recent version of llvm should work for the linter.

### Prepare build

The build needs to be configured with the brew llvm version of `clang` rather than the `clang` packaged with Mac, you can point cmake to the chosen one by doing (e.g. on Mac):

```bash
export CC=/opt/homebrew/opt/llvm@14/bin/clang
export CXX=/opt/homebrew/opt/llvm@14/bin/clang++
```

Now you can run cmake in with a **clean** build directory (`$BUILD_DIR`) in the same shell session. It needs to exports its compile commands, so do:

```bash
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $SOURCE_DIR
```

### Run

You can do the following in the source directory, pointing to the `$BUILD_DIR`:

```bash
tools/run_lint.sh $BUILD_DIR
```




