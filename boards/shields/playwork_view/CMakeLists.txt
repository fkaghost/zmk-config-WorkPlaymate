if(CONFIG_ZMK_DISPLAY AND CONFIG_NICE_VIEW_WIDGET_STATUS)
  # Reuse the proven canvas helpers and charging-bolt asset from the
  # stock ZMK v0.3 nice_view implementation.
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/boards/shields/nice_view/widgets)

  zephyr_library_sources(custom_status_screen.c)
  zephyr_library_sources(widgets/status.c)

  zephyr_library_sources(${CMAKE_SOURCE_DIR}/boards/shields/nice_view/widgets/bolt.c)
  zephyr_library_sources(${CMAKE_SOURCE_DIR}/boards/shields/nice_view/widgets/util.c)
endif()
