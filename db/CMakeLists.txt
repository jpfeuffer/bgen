add_library(bgen_db STATIC
    src/Connection.cpp
    src/Error.cpp
    src/SQLStatement.cpp
    src/SQLite3Connection.cpp
    src/SQLite3Statement.cpp
)
add_library(bgen::db ALIAS bgen_db)

target_include_directories(bgen_db
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(bgen_db PUBLIC cxx_std_17)
target_link_libraries(bgen_db
    PUBLIC
        SQLite3::SQLite3
    PRIVATE
        Boost::date_time
        Boost::thread
)
