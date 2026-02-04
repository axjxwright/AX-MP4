if( NOT TARGET AX-MP4 )

	option( AX_MEDIA_WITH_MKV "Compile with MKV Container support" ON )
	option( AX_MEDIA_WITH_AVI "Compile with AVI Container support" OFF )

	get_filename_component( AXMP4_SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../src" ABSOLUTE )
	get_filename_component( CINDER_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE )

	file ( GLOB_RECURSE AXMP4_SOURCE_FILES "${AXMP4_SOURCE_PATH}/*.h" "${AXMP4_SOURCE_PATH}/*.cxx" )
	add_library( AX-MP4 ${AXMP4_SOURCE_FILES} )

	target_include_directories( AX-MP4 PUBLIC "${AXMP4_SOURCE_PATH}" )
	target_include_directories( AX-MP4 SYSTEM BEFORE PUBLIC "${CINDER_PATH}/include" )

	if(AX_MEDIA_WITH_MKV)
		target_compile_definitions( AX-MP4 PUBLIC "-DAX_MEDIA_WITH_MKV")
	endif()

	if(AX_MEDIA_WITH_AVI)
		target_compile_definitions( AX-MP4 PUBLIC "-DAX_MEDIA_WITH_AVI")
	endif()

	source_group( TREE ${AXMP4_SOURCE_PATH} FILES ${AXMP4_SOURCE_FILES} )

	if( NOT TARGET cinder )
		include( "${CINDER_PATH}/proj/cmake/configure.cmake" )
		find_package( cinder REQUIRED PATHS "${CINDER_PATH}/${CINDER_LIB_DIRECTORY}" "$ENV{CINDER_PATH}/${CINDER_LIB_DIRECTORY}" )
	endif()

	target_link_libraries( AX-MP4 PRIVATE cinder )

	if ( WIN32 )
		set_property(TARGET AX-MP4 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif ( )
	
endif()



