cmake_minimum_required(VERSION 3.12...3.25) # Nutzt moderne Policies
project(modbuspp VERSION 1.0.0 LANGUAGES CXX)

# 1. HEADER-ONLY LIBRARY TARGET
# Da dein Projekt offenbar nur aus "modbus.h" besteht, definieren wir eine INTERFACE Library.
add_library(modbuspp INTERFACE)
add_library(modbuspp::modbuspp ALIAS modbuspp) # Namespace-Alias für bessere Einbindung

target_compile_features(modbuspp INTERFACE cxx_std_11)

if(WIN32)
    target_link_libraries(modbuspp INTERFACE ws2_32 mswsock)
endif()

target_include_directories(modbuspp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# 2. COMPILER WARNINGS (Target-basiert)
# Wir definieren ein internes Helfer-Target für Compiler-Optionen, um Redundanz zu vermeiden
add_library(project_warnings INTERFACE)
if(MSVC)
    target_compile_options(project_warnings INTERFACE /W4)
else()
    target_compile_options(project_warnings INTERFACE -Wall)
    if(NOT WIN32)
        target_compile_options(project_warnings INTERFACE -Werror)
    endif()
endif()

# 3. BUILD EXAMPLE
add_executable(modbuspp_example example/example.cpp)

# Wir verknüpfen das Beispiel mit unserer Library und den Warnings
target_link_libraries(modbuspp_example 
    PRIVATE 
        modbuspp 
        project_warnings
)

target_compile_definitions(modbuspp_example PRIVATE ENABLE_MODBUSPP_LOGGING)

# 4. INSTALLATION
include(GNUInstallDirs) # Standardisiert Pfade (include, lib, bin)

install(TARGETS modbuspp EXPORT modbuspp-targets)
install(FILES modbus.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/modbuspp)

install(EXPORT modbuspp-targets
    NAMESPACE modbuspp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/modbuspp
)