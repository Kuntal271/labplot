add_executable (InterpolationTest InterpolationTest.cpp)

target_link_libraries(InterpolationTest labplot2backendlib labplot2lib labplot2nsllib labplot2test)

add_test(NAME InterpolationTest COMMAND InterpolationTest)
