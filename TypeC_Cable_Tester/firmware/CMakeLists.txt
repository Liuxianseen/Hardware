cmake_minimum_required(VERSION 3.20)
project(typec_cable_tester_core C)

add_library(typec_cable_tester_core STATIC
    src/cable_analysis.c
    src/cable_profile.c
    src/pcal6524.c
    src/tester_app.c
    src/tester_report.c
    src/tester_scan.c
    src/tester_types.c
)

target_include_directories(typec_cable_tester_core PUBLIC include)
target_compile_features(typec_cable_tester_core PUBLIC c_std_11)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(typec_cable_tester_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
elseif(MSVC)
    target_compile_options(typec_cable_tester_core PRIVATE /W4 /WX)
endif()
