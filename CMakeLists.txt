cmake_minimum_required(VERSION 3.16)

idf_component_register(
  SRCS "src/ADS1115.cpp"
  INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
