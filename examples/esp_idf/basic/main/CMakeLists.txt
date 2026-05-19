idf_component_register(
  SRCS "main.cpp" "Ads1115IdfI2cTransport.cpp"
  INCLUDE_DIRS "."
  REQUIRES ADS1115 esp_driver_i2c esp_timer freertos log
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
