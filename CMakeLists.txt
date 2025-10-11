cmake_minimum_required(VERSION 3.16)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)	# required for editor

# C++ Compiler Settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set Module Path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Add Libraries
#add_library(PROJECT_HEADER STATIC )

#find_package(sdbus-c++ REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(LIBEVDEV REQUIRED libevdev)

file(GLOB UNIKEY_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

project(unikey)
add_executable(unikey ${UNIKEY_SOURCES})
target_include_directories(unikey
	PRIVATE
		${LIBEVDEV_INCLUDE_DIRS}
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(unikey 
	PRIVATE
		${LIBEVDEV_LIBRARIES}
#		SDBusCpp::sdbus-c++
)