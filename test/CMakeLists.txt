cmake_minimum_required (VERSION 3.8)

file(GLOB tests CONFIGURE_DEPENDS 
     "test_*.c"
     "test_*.cpp"
)

foreach (testsourcefile ${tests})
    get_filename_component(filename ${testsourcefile} NAME_WLE)
    string(SUBSTRING ${filename} 5 -1 testname)
    #message("${testname} | ${testsourcefile}")
    add_executable("${filename}" ${testsourcefile})
    add_test(NAME "NATBuster.${testname}" COMMAND ${filename})
    target_link_libraries("${filename}" OpenSSL::SSL OpenSSL::Crypto Common)
    
endforeach (testsourcefile ${srcs})

include_directories(Client ${ROOT_INCLUDE_DIRS})