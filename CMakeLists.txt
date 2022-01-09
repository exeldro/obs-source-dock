project(source-dock VERSION 0.2.4)
set(PROJECT_FULL_NAME "Source Dock")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)

set(source-dock_HEADERS
	source-dock.hpp
	source-dock-settings.hpp
	qt-display.hpp
	media-control.hpp
	media-slider.hpp
	volume-meter.hpp
	../../slider-absoluteset-style.hpp
	version.h)
set(source-dock_SOURCES
	source-dock.cpp
	source-dock-settings.cpp
	qt-display.cpp
	media-control.cpp
	media-slider.cpp
	volume-meter.cpp
	../../slider-absoluteset-style.cpp)

if(WIN32)
	get_filename_component(ISS_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_FILES_DIR}" ISS_FILES_DIR)
	get_filename_component(ISS_PACKAGE_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_PACKAGE_DIR}" ISS_PACKAGE_DIR)
	get_filename_component(ISS_SOURCE_DIR "${PROJECT_SOURCE_DIR}" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_SOURCE_DIR}" ISS_SOURCE_DIR)
	configure_file("installer.iss.in"
			"${PROJECT_BINARY_DIR}/installer.iss"
		)
	configure_file(resource.rc.in source-dock.rc)
	list(APPEND source-dock_SOURCES
		source-dock.rc)
endif()

add_library(source-dock MODULE
	${source-dock_HEADERS}
	${source-dock_SOURCES})

target_link_libraries(source-dock
		obs-frontend-api
		Qt5::Widgets
		libobs)

set_target_properties(source-dock PROPERTIES FOLDER "plugins/exeldro")
install_obs_plugin_with_data(source-dock data)
