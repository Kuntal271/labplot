option(ENABLE_TEST_IMPORTEXPORT_BINARY "Enable Binary Tests" ON)

if (ENABLE_TEST_IMPORTEXPORT_BINARY)

add_executable (BinaryFilterTest BinaryFilterTest.cpp)

target_link_libraries(BinaryFilterTest labplot2backendlib labplot2lib labplot2nsllib labplot2test)

add_test(NAME BinaryFilterTest COMMAND BinaryFilterTest)

endif()
