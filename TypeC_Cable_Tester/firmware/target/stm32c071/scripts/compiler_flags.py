Import("env")

# STM32CubeC0 LL ADC headers still use the deprecated C++ `register` keyword.
# Suppress only that upstream C++ warning; C compilation diagnostics stay intact.
env.Append(CXXFLAGS=["-Wno-register"])
