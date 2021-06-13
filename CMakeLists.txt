cmake_minimum_required(VERSION 3.19)
project(Interpretation_for_Mython)

set(CMAKE_CXX_STANDARD 17)

set(MAIN_FILE main.cpp)
set(LEXER lexer.h lexer.cpp lexer_test_open.cpp)
set(RUNTIME runtime.h runtime.cpp)
set(PARSE parse.h parse.cpp)
set(STATEMENT statement.h statement.cpp)
set(TESTS test_runner_p.h lexer_test_open.cpp runtime_test.cpp parse_test.cpp statement_test.cpp)

add_executable(Interpretation ${MAIN_FILE} ${LEXER} ${RUNTIME} ${PARSE} ${STATEMENT} ${TESTS})