if( NOT TARGET AX-MP4 )

	get_filename_component( AXMP4_SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../src" ABSOLUTE )
	get_filename_component( CINDER_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE )

	list( APPEND AXMP4_SOURCE_FILES "${AXMP4_SOURCE_PATH}/AX/AX-MP4.h" "${AXMP4_SOURCE_PATH}/AX/AX-MP4.cxx" )

	add_library( AX-MP4 ${AXMP4_SOURCE_FILES} )

	target_include_directories( AX-MP4 PUBLIC "${AXMP4_SOURCE_PATH}" )
	target_include_directories( AX-MP4 SYSTEM BEFORE PUBLIC "${CINDER_PATH}/include" )

	if( NOT TARGET cinder )
		include( "${CINDER_PATH}/proj/cmake/configure.cmake" )
		find_package( cinder REQUIRED PATHS "${CINDER_PATH}/${CINDER_LIB_DIRECTORY}" "$ENV{CINDER_PATH}/${CINDER_LIB_DIRECTORY}" )
	endif()

	target_link_libraries( AX-MP4 PRIVATE cinder )

	if ( WIN32 )
		set_property(TARGET AX-MP4 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif ( )
	
endif()



