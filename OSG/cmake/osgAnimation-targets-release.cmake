#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "osg3::osgAnimation" for configuration "Release"
set_property(TARGET osg3::osgAnimation APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(osg3::osgAnimation PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/osgAnimation.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/osg202-osgAnimation.dll"
  )

list(APPEND _cmake_import_check_targets osg3::osgAnimation )
list(APPEND _cmake_import_check_files_for_osg3::osgAnimation "${_IMPORT_PREFIX}/lib/osgAnimation.lib" "${_IMPORT_PREFIX}/bin/osg202-osgAnimation.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
