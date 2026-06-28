# nanobind Python module. Driven by scikit-build-core (pyproject.toml) for wheels.
find_package(Python 3.9 COMPONENTS Interpreter Development.Module REQUIRED)

# Locate nanobind's CMake config via the installed pip package.
execute_process(
  COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE NB_DIR)
list(APPEND CMAKE_PREFIX_PATH "${NB_DIR}")
find_package(nanobind CONFIG REQUIRED)

nanobind_add_module(_core STABLE_ABI NB_STATIC mudraka_ext.cpp)
target_link_libraries(_core PRIVATE mudraka_core)

# Installed into the `mudraka` package (see python/mudraka/__init__.py).
install(TARGETS _core LIBRARY DESTINATION mudraka)
