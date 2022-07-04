cmake_minimum_required (VERSION 3.8)

file(GLOB tests CONFIGURE_DEPENDS 
     "test_*.c"
     "test_*.cpp"
)

foreach (testsourcefile ${tests})
    get_filename_component(testname ${testsourcefile} NAME_WLE)
    message("${testname} | ${testsourcefile}")
    add_executable("${testname}" ${testsourcefile})
    add_test(NAME ${testname} COMMAND ${testname})
    target_link_libraries("${testname}" OpenSSL::SSL OpenSSL::Crypto Common)
endforeach (testsourcefile ${srcs})

include_directories(Client ${ROOT_INCLUDE_DIRS})