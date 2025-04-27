cmake_minimum_required(VERSION 3.10)
project(Test)

find_package(html_forms_server REQUIRED)

add_executable(test test.cpp)
target_link_libraries(test html_forms_server)
