cmake_minimum_required (VERSION 3.7.2)
project (netstore-2 CXX)

find_package ( Threads REQUIRED )
find_package ( Boost COMPONENTS program_options REQUIRED)

include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

#set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Wextra -O2 -std=c++17 -l stdc++fs")

add_library(nssockets src/sockets.cc)
add_library(nsaux src/aux.cc)
add_library(nscmd src/cmd.cc)
target_link_libraries(nsaux stdc++fs)

# http://stackoverflow.com/questions/10555706/
macro (add_executable _name)
    # invoke built-in add_executable
    _add_executable(${ARGV})
    if (TARGET ${_name})
        target_link_libraries(${_name} ${CMAKE_THREAD_LIBS_INIT}
                ${Boost_LIBRARIES} stdc++fs nssockets nsaux nscmd)
    endif()
endmacro()

add_executable(netstore-server src/server.cc)
add_executable(netstore-client src/client.cc)


install(TARGETS DESTINATION .)
