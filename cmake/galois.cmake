include(FetchContent)

FetchContent_Declare(
  com_intelligentsoftwaresystems_galois
  GIT_REPOSITORY https://github.com/IntelligentSoftwareSystems/Galois
  GIT_TAG        release-6.0
)
FetchContent_MakeAvailable(com_intelligentsoftwaresystems_galois)

if(IS_DIRECTORY "${com_intelligentsoftwaresystems_galois_SOURCE_DIR}")
    set_property(DIRECTORY ${com_intelligentsoftwaresystems_galois_SOURCE_DIR} PROPERTY EXCLUDE_FROM_ALL YES)
endif()

include_directories(
  ${com_intelligentsoftwaresystems_galois_SOURCE_DIR}/include
)