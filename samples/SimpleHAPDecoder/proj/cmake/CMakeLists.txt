cmake_minimum_required( VERSION 3.10 FATAL_ERROR )
set( CMAKE_VERBOSE_MAKEFILE ON )
project( SimpleHAPDecoderApp )

option(AX_LIVEPP_ENABLED "Enable LivePP" ON)

get_filename_component( APP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../" ABSOLUTE )
get_filename_component( CINDER_PATH "${APP_PATH}/../../../../" ABSOLUTE )

# Newer version of cinder differ in how they call
# target_link_libraries which causes a cmake error
# on different version of cinder. This patches and
# corrects it
function(target_link_libraries target)
    if(ARGV1 MATCHES "^(PRIVATE|PUBLIC|INTERFACE)$")
        _target_link_libraries(${target} ${ARGN})
    else()
        _target_link_libraries(${target} PUBLIC ${ARGN})
        # message(AUTHOR_WARNING "target_link_libraries: missing PRIVATE, PUBLIC or INTERFACE for target ${target}. Defaulting to PUBLIC.")
    endif()
endfunction()

if ( WIN32 )
	set(CMAKE_CXX_STANDARD 20)
	add_compile_options(/W4)

	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
	
endif()

include("${CINDER_PATH}/proj/cmake/modules/cinderMakeApp.cmake")

set(THE_BLOCKS "AX-MP4")

ci_make_app(
	SOURCES "${APP_PATH}/src/SimpleHAPDecoderApp.cxx"
	CINDER_PATH ${CINDER_PATH}
	BLOCKS ${THE_BLOCKS}
)

if ( WIN32 )

	target_compile_features( ${PROJECT_NAME} PRIVATE cxx_std_20)
	set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	
	if ( AX_LIVEPP_ENABLED )
		if(EXISTS "${APP_PATH}/assets/LivePP/API/x64/LPP_API_x64_CPP.h")
		    target_link_options(${PROJECT_NAME} PUBLIC "/FUNCTIONPADMIN")
			target_link_options(${PROJECT_NAME} PUBLIC "/OPT:NOREF")
			target_link_options(${PROJECT_NAME} PUBLIC "/OPT:NOICF")
			target_link_options(${PROJECT_NAME} PUBLIC "/DEBUG:FULL")
			target_compile_definitions( ${PROJECT_NAME} PUBLIC "-DAX_LIVEPP_ENABLED")
			target_include_directories ( ${PROJECT_NAME} PUBLIC "${APP_PATH}/assets/LivePP/API/x64")
		else()
		    message(WARNING "Live++ is enabled but cannot find the API headers. Disabling.")
			set(AX_LIVEPP_ENABLED OFF FORCE)
		endif()
	endif()
endif()

set(SNAPPY_BUILD_TESTS OFF CACHE BOOL "Don't build snappy tests")
set(SNAPPY_BUILD_BENCHMARKS OFF CACHE BOOL "Don't build snappy benchmarks")

add_subdirectory("${APP_PATH}/lib/snappy" "${CMAKE_BINARY_DIR}/snappy")

add_library(hap STATIC "${APP_PATH}/lib/hap/source/hap.h" "${APP_PATH}/lib/hap/source/hap.c")
target_include_directories(hap PUBLIC "${APP_PATH}/lib/snappy")
add_dependencies(hap snappy)

target_include_directories(${PROJECT_NAME} PUBLIC "${APP_PATH}/lib/hap/source")
target_link_libraries(${PROJECT_NAME} PUBLIC hap)
target_link_libraries(${PROJECT_NAME} PUBLIC snappy)