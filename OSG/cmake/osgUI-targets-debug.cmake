#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "osg3::osgUI" for configuration "Debug"
set_property(TARGET osg3::osgUI APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(osg3::osgUI PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/lib/osgUId.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/osg202-osgUId.dll"
  )

list(APPEND _cmake_import_check_targets osg3::osgUI )
list(APPEND _cmake_import_check_files_for_osg3::osgUI "${_IMPORT_PREFIX}/lib/osgUId.lib" "${_IMPORT_PREFIX}/bin/osg202-osgUId.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
