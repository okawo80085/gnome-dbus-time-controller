project(gnomeDbusTimeServer)

add_custom_target(generate-dbus-adaptors ALL)

add_custom_command(
        TARGET generate-dbus-adaptors
        PRE_BUILD
        COMMAND dbusxx-xml2cpp
        ARGS ${CMAKE_SOURCE_DIR}/src/gnome-dbus-time-service.xml --adaptor=${CMAKE_SOURCE_DIR}/src/gnome-dbus-time-service-glue.hpp
)

file(
        GLOB_RECURSE
        SRC
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.hpp"
        # "${CMAKE_SOURCE_DIR}/src/utils/*.cpp"
        # "${CMAKE_SOURCE_DIR}/src/utils/*.hpp"
)
# list(FILTER SRC EXCLUDE REGEX "<pattern>")


add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} ${DBUS_LDFLAGS_OTHER})
target_link_libraries(${PROJECT_NAME} ${DBUS_LIBRARIES})