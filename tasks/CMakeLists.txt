

SET(libname logger-tasks-${OROCOS_TARGET})
ADD_LIBRARY(${libname} SHARED ${CMAKE_SOURCE_DIR}/.orogen/tasks/LoggerBase.cpp Logger.cpp Logfile.cpp)
TARGET_APPEND_LDFLAGS(${libname} "${DEPENDENCIES_LDFLAGS}")

INSTALL(TARGETS ${libname}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib/orocos)

INSTALL(FILES ${CMAKE_SOURCE_DIR}/.orogen/tasks/LoggerBase.cpp Logger.cpp ${CMAKE_SOURCE_DIR}/.orogen/tasks/LoggerBase.hpp Logger.hpp
    DESTINATION include/orocos/logger)
