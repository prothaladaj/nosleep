cmake_minimum_required(VERSION 3.20)
project(NoSleepLid LANGUAGES CXX)

add_executable(nosleep WIN32 src/main.cpp)
target_compile_features(nosleep PRIVATE cxx_std_17)
target_compile_definitions(nosleep PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(nosleep PRIVATE user32 shell32 advapi32 gdi32 powrprof)

if(MSVC)
  target_compile_options(nosleep PRIVATE /O1 /GS- /GR- /EHsc /utf-8)
  target_link_options(nosleep PRIVATE /OPT:REF /OPT:ICF)
else()
  target_compile_options(nosleep PRIVATE -finput-charset=UTF-8)
endif()
