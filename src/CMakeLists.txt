include(CheckIncludeFile)

include(CheckSymbolExists)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

if(MSVC)
list(APPEND MSVC_APP getopt/getopt.c)
endif(MSVC)

add_executable(intraportmap crc32.cpp intraportmap.cpp ipm_client.cpp ipm_client_tunnel.cpp ipm_server.cpp ipm_server_agent.cpp ipm_server_tunnel.cpp main.cpp slog.cpp stdafx.cpp util.cpp ${MSVC_APP})

target_link_libraries(intraportmap libevent::extra)

install(TARGETS intraportmap DESTINATION /usr/sbin)


