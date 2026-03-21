#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "osg3::osgUtil" for configuration "Debug"
set_property(TARGET osg3::osgUtil APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(osg3::osgUtil PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/lib/osgUtild.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/osg202-osgUtild.dll"
  )

list(APPEND _cmake_import_check_targets osg3::osgUtil )
list(APPEND _cmake_import_check_files_for_osg3::osgUtil "${_IMPORT_PREFIX}/lib/osgUtild.lib" "${_IMPORT_PREFIX}/bin/osg202-osgUtild.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
