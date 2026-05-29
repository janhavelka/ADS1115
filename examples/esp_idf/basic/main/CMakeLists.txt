idf_component_register(
  SRCS "main.cpp"
  INCLUDE_DIRS "."
  REQUIRES ADS1115 esp_driver_i2c esp_timer freertos
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
