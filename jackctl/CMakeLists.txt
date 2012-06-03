set(QT_DONT_USE_QTGUI 1)
set(QT_USE_QTDBUS 1)

set(kdenlive_jackctl_SRCS
  kdenlive_jackctl.cpp
)

kde4_add_executable(kdenlive_jackctl ${kdenlive_jackctl_SRCS})

include_directories(
  ${QT_INCLUDES}
  ${LIBMLT_INCLUDE_DIR}
  ${LIBMLTPLUS_INCLUDE_DIR}
)

include(${QT_USE_FILE})

target_link_libraries(kdenlive_jackctl
  ${QT_LIBRARIES}
  ${LIBMLT_LIBRARY}
  ${LIBMLTPLUS_LIBRARY}
  jack
)

install(TARGETS kdenlive_jackctl DESTINATION ${BIN_INSTALL_DIR})
