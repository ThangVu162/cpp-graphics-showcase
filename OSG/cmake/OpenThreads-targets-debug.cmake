#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "osg3::OpenThreads" for configuration "Debug"
set_property(TARGET osg3::OpenThreads APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(osg3::OpenThreads PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/lib/OpenThreadsd.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/ot21-OpenThreadsd.dll"
  )

list(APPEND _cmake_import_check_targets osg3::OpenThreads )
list(APPEND _cmake_import_check_files_for_osg3::OpenThreads "${_IMPORT_PREFIX}/lib/OpenThreadsd.lib" "${_IMPORT_PREFIX}/bin/ot21-OpenThreadsd.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
