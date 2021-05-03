include(FetchContent)

FetchContent_Declare(
  com_intelligentsoftwaresystems_galois
  GIT_REPOSITORY https://github.com/IntelligentSoftwareSystems/Galois
  GIT_TAG        master
)
FetchContent_MakeAvailable(com_intelligentsoftwaresystems_galois)

include_directories(
  ${com_intelligentsoftwaresystems_galois_SOURCE_DIR}/include
)
