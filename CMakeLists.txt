cmake_minimum_required(VERSION 3.10)

option(USE_PNG "Enable PNG output" ON)

project(meteor_decode
	VERSION 1.0.1
	DESCRIPTION "LRPT decoder"
	LANGUAGES C)
add_definitions(-DVERSION="${CMAKE_PROJECT_VERSION}")

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe -Wextra -Wimplicit-fallthrough")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=native -ftree-vectorize")

# ARM architectures need -mfpu=auto in order to enable NEON when available,
# but that option is unrecognized by x86 gcc (and possibly others): only
# add it to the release flags when the compiler's target is arm
# This is not a problem for arm64, as NEON support is mandatory for that arch
execute_process(COMMAND "${CMAKE_C_COMPILER}" "-dumpmachine" COMMAND "grep" "arm" OUTPUT_QUIET RESULT_VARIABLE is_arm)
if (is_arm EQUAL "0")
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -mfpu=auto")
endif()



set(COMMON_SOURCES
	correlator/correlator.c correlator/correlator.h
	correlator/autocorrelator.c correlator/autocorrelator.h
	deinterleave/deinterleave.c deinterleave/deinterleave.h
	diffcode/diffcode.c diffcode/diffcode.h
	ecc/descramble.c ecc/descramble.h
	ecc/rs.c ecc/rs.h
	ecc/viterbi.c ecc/viterbi.h
	jpeg/huffman.c jpeg/huffman.h
	jpeg/jpeg.c jpeg/jpeg.h
	math/int.c math/int.h
	parser/mcu_parser.c parser/mcu_parser.h
	parser/mpdu_parser.c parser/mpdu_parser.h
	protocol/mcu.c protocol/mcu.h
	protocol/mpdu.c protocol/mpdu.h
	protocol/vcdu.c protocol/vcdu.h
	output/bmp_out.c output/bmp_out.h
	channel.c channel.h
	raw_channel.c raw_channel.h
	decode.c decode.h
	utils.c utils.h
)

set(COMMON_INC_DIRS
	${PROJECT_SOURCE_DIR}
	correlator/ deinterleave/ diffcode/ ecc/ jpeg/ math/ parser/ protocol/
)


# Enable PNG if requested at configure time AND libpng is present
if (USE_PNG)
	find_library(PNG_LIBRARY NAMES png libpng)
	if (PNG_LIBRARY)
		add_definitions(-DUSE_PNG)
		set(COMMON_SOURCES ${COMMON_SOURCES} output/png_out.c output/png_out.h)
	else()
		message(WARNING "libpng not found, PNG output will be unavailable")
	endif()
endif()

# Main executable target
add_executable(meteor_decode main.c ${COMMON_SOURCES})
target_include_directories(meteor_decode PUBLIC ${COMMON_INC_DIRS})

# Add links to PNG library if enabled
if(USE_PNG AND PNG_LIBRARY)
	target_link_libraries(meteor_decode PUBLIC png)
endif()

# Install targets
install(TARGETS meteor_decode DESTINATION bin)

# uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

