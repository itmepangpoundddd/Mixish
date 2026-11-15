cmake_minimum_required(VERSION 3.28...3.30)

add_library(mixish-timer MODULE)

target_sources(mixish-timer PRIVATE mixish-timer.cpp)

target_link_libraries(mixish-timer PRIVATE OBS::libobs gdiplus)

set_target_properties_obs(mixish-timer PROPERTIES FOLDER plugins PREFIX "")
