add_executable (NSLPeakTest NSLPeakTest.cpp)

target_link_libraries(NSLPeakTest labplot2nsllib labplot2test)

add_test(NAME NSLPeakTest COMMAND NSLPeakTest)
