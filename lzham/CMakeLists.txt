
set(lzham_SRC
lzham.h
lzham_comp.h
lzham_lzbase.cpp
lzham_lzbase.h
lzham_lzcomp.cpp
lzham_lzcomp_internal.cpp
lzham_lzcomp_internal.h
lzham_match_accel.cpp
lzham_match_accel.h
lzham_null_threading.h
lzham_pthreads_threading.cpp
lzham_pthreads_threading.h
lzham_threading.h
lzham_win32_threading.cpp
lzham_win32_threading.h
lzham_assert.cpp
lzham_assert.h
lzham_checksum.cpp
lzham_checksum.h
lzham_config.h
lzham_core.h
lzham_decomp.h
lzham_helpers.h
lzham_huffman_codes.cpp
lzham_huffman_codes.h
lzham_lzdecomp.cpp
lzham_lzdecompbase.cpp
lzham_lzdecompbase.h
lzham_math.h
lzham_mem.cpp
lzham_mem.h
lzham_platform.cpp
lzham_platform.h
lzham_polar_codes.cpp
lzham_polar_codes.h
lzham_prefix_coding.cpp
lzham_prefix_coding.h
lzham_symbol_codec.cpp
lzham_symbol_codec.h
lzham_timer.cpp
lzham_timer.h
lzham_traits.h
lzham_types.h
lzham_utils.h
lzham_vector.cpp
lzham_vector.h
)

if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
endif()

add_library(lzham ${lzham_SRC})

if(NOT WIN32)
        target_link_libraries(lzham pthread)
endif()

