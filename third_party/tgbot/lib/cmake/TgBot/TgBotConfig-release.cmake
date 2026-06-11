#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "TgBot::TgBot" for configuration "Release"
set_property(TARGET TgBot::TgBot APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(TgBot::TgBot PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/libTgBot.dll.a"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/libTgBot.dll"
  )

list(APPEND _cmake_import_check_targets TgBot::TgBot )
list(APPEND _cmake_import_check_files_for_TgBot::TgBot "${_IMPORT_PREFIX}/lib/libTgBot.dll.a" "${_IMPORT_PREFIX}/bin/libTgBot.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
