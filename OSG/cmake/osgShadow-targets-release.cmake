#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "osg3::osgShadow" for configuration "Release"
set_property(TARGET osg3::osgShadow APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(osg3::osgShadow PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/osgShadow.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/osg202-osgShadow.dll"
  )

list(APPEND _cmake_import_check_targets osg3::osgShadow )
list(APPEND _cmake_import_check_files_for_osg3::osgShadow "${_IMPORT_PREFIX}/lib/osgShadow.lib" "${_IMPORT_PREFIX}/bin/osg202-osgShadow.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
