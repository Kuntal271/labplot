add_executable (OdsFilterTest OdsFilterTest.cpp)

target_link_libraries(OdsFilterTest labplot2backendlib labplot2lib labplot2nsllib labplot2test)

add_test(NAME OdsFilterTest COMMAND OdsFilterTest)
