add_executable(test_bgen
    unit/main.cpp
    unit/test_little_endian.cpp
    unit/ProbSetCheck.cpp
    unit/test_utils.cpp
    unit/test_bgen_header_format.cpp
    unit/test_bgen_snp_format.cpp
    unit/test_variant_data_block.cpp
)
target_include_directories(test_bgen PRIVATE
    unit
    ${CMAKE_BINARY_DIR}
)
target_link_libraries(test_bgen PRIVATE
    bgen::bgen
    bgen::db
    bgen::appcontext
    Boost::filesystem
)

add_test(NAME test_bgen COMMAND test_bgen)
