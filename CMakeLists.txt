if(CONFIG_SHIELD_TORABO_TSUKI_LP_LEFT OR CONFIG_SHIELD_TORABO_TSUKI_LP_RIGHT)
  zephyr_library()
  zephyr_include_directories(
    ${APPLICATION_SOURCE_DIR}/include
    drivers/iqs7211e/include
  )
  zephyr_library_sources(src/board.c src/mini_trackpad_init_reg.c)
  zephyr_library_sources_ifdef(
    CONFIG_IQS7211E
    drivers/iqs7211e/src/iqs7211e.c
    drivers/iqs7211e/src/iqs7211e_reg.c
  )
endif()
