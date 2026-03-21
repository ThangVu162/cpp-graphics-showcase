#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "osg3::OpenThreads" for configuration "Release"
set_property(TARGET osg3::OpenThreads APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(osg3::OpenThreads PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/OpenThreads.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/ot21-OpenThreads.dll"
  )

list(APPEND _cmake_import_check_targets osg3::OpenThreads )
list(APPEND _cmake_import_check_files_for_osg3::OpenThreads "${_IMPORT_PREFIX}/lib/OpenThreads.lib" "${_IMPORT_PREFIX}/bin/ot21-OpenThreads.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
