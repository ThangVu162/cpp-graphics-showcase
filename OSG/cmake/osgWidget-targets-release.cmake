#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "osg3::osgWidget" for configuration "Release"
set_property(TARGET osg3::osgWidget APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(osg3::osgWidget PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/osgWidget.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/osg202-osgWidget.dll"
  )

list(APPEND _cmake_import_check_targets osg3::osgWidget )
list(APPEND _cmake_import_check_files_for_osg3::osgWidget "${_IMPORT_PREFIX}/lib/osgWidget.lib" "${_IMPORT_PREFIX}/bin/osg202-osgWidget.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
