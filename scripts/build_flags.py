# Adds flags that PlatformIO's build_flags option can't route correctly.
# Runs before the build, via extra_scripts in platformio.ini.

Import("env")

FPU_FLAGS = ["-mfpu=fpv4-sp-d16", "-mfloat-abi=hard"]

env.Append(
    # build_flags sends these to the compiler but not the linker or assembler,
    # which makes the link fail with "uses VFP register arguments".
    CCFLAGS=FPU_FLAGS + ["-g"],
    ASFLAGS=FPU_FLAGS,
    LINKFLAGS=FPU_FLAGS + ["-Wl,-Map,${BUILD_DIR}/firmware.map"],
    # build_flags also sends flags to C files, where C++-only options cause warnings.
    CXXFLAGS=["-fno-threadsafe-statics"],
)
