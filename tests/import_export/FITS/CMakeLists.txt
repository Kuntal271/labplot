add_executable (FITSFilterTest FITSFilterTest.cpp)

target_link_libraries(FITSFilterTest labplot2backendlib labplot2lib labplot2nsllib labplot2test)

add_test(NAME FITSFilterTest COMMAND FITSFilterTest)
