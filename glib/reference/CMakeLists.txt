add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/glib-docs-build.stamp
  DEPENDS poppler-glib
  COMMAND ${PROJECT_SOURCE_DIR}/make-glib-api-docs --src-dir=${PROJECT_SOURCE_DIR} --build-dir=${PROJECT_BINARY_DIR}
  COMMAND touch ${CMAKE_CURRENT_BINARY_DIR}/glib-docs-build.stamp
)

add_custom_target(glib-docs ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/glib-docs-build.stamp)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/
  DESTINATION "${CMAKE_INSTALL_DATADIR}/gtk-doc/html/poppler"
)
