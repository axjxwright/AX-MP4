cmake_minimum_required( VERSION 3.10 FATAL_ERROR )
set( CMAKE_VERBOSE_MAKEFILE ON )
project( AACExtractorApp )

option(AX_LIVEPP_ENABLED "Enable LivePP" ON)

get_filename_component( APP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../" ABSOLUTE )
get_filename_component( CINDER_PATH "${APP_PATH}/../../../../" ABSOLUTE )

if ( WIN32 )
	set(CMAKE_CXX_STANDARD 20)
	add_compile_options(/W4 /nologo)
endif()

include("${CINDER_PATH}/proj/cmake/modules/cinderMakeApp.cmake")

set(THE_BLOCKS "AX-MP4")

ci_make_app(
	SOURCES "${APP_PATH}/src/AACExtractorApp.cxx"
	CINDER_PATH ${CINDER_PATH}
	BLOCKS ${THE_BLOCKS}
)

if ( WIN32 )
	target_compile_features( ${PROJECT_NAME} PRIVATE cxx_std_20)
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

#target_include_directories(${PROJECT_NAME} PUBLIC "C:\\Dev\\SDKs\\soloud\\include")
#target_link_libraries(${PROJECT_NAME} "C:\\Dev\\SDKs\\soloud\\build\\vs2026\\x64\\Debug\\soloud_static.lib")