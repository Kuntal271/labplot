add_executable (DifferentiationTest DifferentiationTest.cpp)

target_link_libraries(DifferentiationTest labplot2backendlib labplot2lib labplot2nsllib labplot2test)

add_test(NAME DifferentiationTest COMMAND DifferentiationTest)
