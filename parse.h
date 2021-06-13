#pragma once

#include <memory>
#include <stdexcept>

namespace parse {
    class Lexer;

    struct ParseError : std::runtime_error {
        using std::runtime_error::runtime_error;
    };
}

namespace runtime {
    class Executable;
}

std::unique_ptr<runtime::Executable> ParseProgram(parse::Lexer& lexer);