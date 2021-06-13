#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os, const Token& rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
        if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
        if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(std::istream& input)
            : input_(input) {
        while (input_.peek() == '\n') {
            input_.get();
        }
        ParseToken();
    }

    const Token& Lexer::CurrentToken() const {
        return current_token_;
    }

    Token Lexer::NextToken() {
        ParseToken();
        return CurrentToken();
    }

    void Lexer::ParseToken() {
        if (current_token_ == token_type::Eof{}) {
            return;
        }
        if (input_.eof() || input_.peek() == EOF) {
            if (count_indent_ > 0) {
                current_token_ = token_type::Dedent{};
                count_indent_ -= 2;
                current_token_ = token_type::Dedent{};
            } else if (current_token_ != token_type::Newline{} && current_token_ != token_type::Dedent{}) {
                current_token_ = token_type::Newline{};
            } else {
                current_token_ = token_type::Eof{};
            }
            return;
        }

        if (dedent_count_ > 0) {
            --dedent_count_;
            count_indent_-=2;
            current_token_ = token_type::Dedent{};
            return;
        }
        char ch = static_cast<char>(input_.get());
        if (ch == '#') {
            std::string temp;
            getline(input_, temp);
            current_token_ = token_type::Newline{};
            if (is_start_line_) {
                ParseToken();
            }
            return;
        }
        if (ch == '\n' && current_token_ == token_type::Newline{}) {
            is_start_line_ = true;
            return ParseToken();
        }
        if (is_start_line_ && count_indent_ > 0 && ch != ' ' && !is_code_block_) {
            input_.putback(ch);
            current_token_ = token_type::Dedent{};
            count_indent_ -= 2;
            return;
        }
        is_code_block_ = false;
        if (ch == '\n') {
            current_token_ = token_type::Newline{};
            is_start_line_ = true;
            return;
        } else if (ch == '\'' || ch == '\"') {
            ParseString(ch);
        } else if (isdigit(ch)) {
            input_.putback(ch);
            ParseNumber();
        } else if (isalpha(ch) || ch == '_') {
            input_.putback(ch);
            ParseIdentifier();
        } else if (ch == ' ') {
            input_.putback(ch);
            ParseIndent();
        } else {
            input_.putback(ch);
            ParseSymbol();
        }
        is_start_line_ = false;
    }

    void Lexer::ParseNumber() {
        std::string parsed_num;
        while (isdigit(input_.peek())) {
            parsed_num += static_cast<char>(input_.get());
        }
        current_token_ = token_type::Number{std::stoi(parsed_num)};
    }

    void Lexer::ParseString(char quote) {
        std::string s;
        for (char ch; (ch = static_cast<char>(input_.get())) != quote;) {
            if (ch == '\\') {
                const char escaped_char = static_cast<char>(input_.get());
                switch (escaped_char) {
                    case 'n':
                        s.push_back('\n');
                        break;
                    case 't':
                        s.push_back('\t');
                        break;
                    case 'r':
                        s.push_back('\r');
                        break;
                    case '"':
                        s.push_back('"');
                        break;
                    case '\'':
                        s.push_back('\'');
                        break;
                    case '\\':
                        s.push_back('\\');
                        break;
                    default:
                        s.push_back(ch);
                }
            } else {
                s.push_back(ch);
            }
        }
        current_token_ = token_type::String{s};
    }

    void Lexer::ParseIdentifier() {
        std::string str;
        while (isalnum(input_.peek()) || input_.peek() == '_') {
            str += static_cast<char>(input_.get());
        }
        if(!ParseKeyword(str)) {
            current_token_ = token_type::Id{str};
        }
    }

    void Lexer::ParseSymbol() {
        char ch = static_cast<char>(input_.get());
        if (ch == '=') {
            if (input_.peek() == '=') {
                current_token_ = token_type::Eq{};
                input_.get();
            } else {
                current_token_ = token_type::Char{ch};
            }
        } else if (ch == '>') {
            if (input_.peek() == '=') {
                current_token_ = token_type::GreaterOrEq{};
                input_.get();
            } else {
                current_token_ = token_type::Char{ch};
            }
        } else if (ch == '<') {
            if (input_.peek() == '=') {
                current_token_ = token_type::LessOrEq{};
                input_.get();
            } else {
                current_token_ = token_type::Char{ch};
            }
        } else if (ch == '!') {
            if (input_.peek() == '=') {
                current_token_ = token_type::NotEq{};
                input_.get();
            }
        } else {
            current_token_ = token_type::Char{ch};
        }
    }

    bool Lexer::ParseKeyword(const std::string& str) {
        if (str == "class") {
            current_token_ = token_type::Class{};
        } else if (str == "return") {
            current_token_ = token_type::Return{};
        } else if (str == "if") {
            current_token_ = token_type::If{};
        } else if (str == "else") {
            current_token_ = token_type::Else{};
        } else if (str == "def") {
            current_token_ = token_type::Def{};
        } else if (str == "print") {
            current_token_ = token_type::Print{};
        } else if (str == "and") {
            current_token_ = token_type::And{};
        } else if (str == "or") {
            current_token_ = token_type::Or{};
        } else if (str == "not") {
            current_token_ = token_type::Not{};
        } else if (str == "None") {
            current_token_ = token_type::None{};
        } else if (str == "True") {
            current_token_ = token_type::True{};
        } else if (str == "False") {
            current_token_ = token_type::False{};
        } else {
            return false;
        }
        return true;
    }

    void Lexer::ParseIndent() {
        if (!current_token_.Is<token_type::Newline>()) {
            input_.get();
            return ParseToken();
        }
        int count_spaces = 0;
        while (input_.peek() == ' ') {
            ++count_spaces;
            input_.get();
        }
        if (count_spaces == count_indent_) {
            is_code_block_ = true;
            return ParseToken();
        } else if (count_spaces - count_indent_ == 2) {
            count_indent_ += 2;
            current_token_ = token_type::Indent{};
        } else {
            int difference = count_indent_ - count_spaces;
            current_token_ = token_type::Dedent{};
            count_indent_ -= 2;
            while (difference > 2) {
                difference-=2;
                ++dedent_count_;
            }
        }

    }

}  // namespace parse