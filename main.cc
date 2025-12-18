#include <print>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <utility>
#include <stdexcept>

#include "Lexer/Lexer.hxx"
#include "Preprocessor/Preprocessor.hxx"
#include "Parser/Parser.hxx"
#include "Interp/Interpreter.hxx"



[[nodiscard]] inline std::string readFile(const std::string& fname) {
    const std::ifstream fin{fname};

    if (not fin.is_open()) error("File \"" + fname + " \"not found!");

    std::stringstream ss;
    ss << fin.rdbuf();

    return ss.str();
}


void REPL(
    const std::filesystem::path canonical_root,
    const bool print_preprocessed,
    const bool print_tokens,
    const bool print_parsed,
    const bool run
) {
    Parser parser{canonical_root};
    Visitor visitor;

    for (;;) try {
        std::string line;
        std::print(">>> ");
        std::getline(std::cin, line);

        constexpr auto REPL = true;
        auto processed_line = preprocess<REPL>(std::move(line), canonical_root); // root in repl mode is where we ran the interpret
        if (print_preprocessed) std::println(std::clog, "{}", processed_line);

        Tokens v = lex(std::move(processed_line));
        if (print_tokens) std::println(std::clog, "{}", v);

        if (v.empty()) continue;

        auto [exprs, ops] = parser.parse(std::move(v));

        if (print_parsed) for(const auto& expr : exprs) std::println(std::clog, "{};", expr->stringify(0));

        if(run and (print_parsed or print_preprocessed or print_tokens)) puts("Output:\n");


        if (run) {
            visitor.addOperators(std::move(ops));

            if (not exprs.empty()) {
                Value value;
                for (auto&& expr : exprs) value = std::visit(visitor, std::move(expr)->variant());

                std::println("{}", stringify(value));
            }
        }
    }
    catch(const std::exception &e) {
        std::clog << e.what() << std::endl;
    }
}


void runFile(
    const std::filesystem::path fname,
    const bool print_preprocessed,
    const bool print_tokens,
    const bool print_parsed,
    const bool run
) {
    auto src = readFile(fname);

    auto processed_src = preprocess(std::move(src), fname);
    if (print_preprocessed) std::println(std::clog, "{}", processed_src);

    Tokens v = lex(std::move(processed_src));
    if (print_tokens) std::println(std::clog, "{}", v);

    if (v.empty()) return;

    Parser p{std::move(v), fname};

    auto [exprs, ops] = p.parse();

    if (print_parsed)
        for(const auto& expr : exprs)
            std::println(std::clog, "{};", expr->stringify(0));

    if(run and (print_parsed or print_preprocessed or print_tokens)) puts("Output:\n");

    if (run) {
        Visitor visitor{std::move(ops)};
        for (auto&& expr : exprs)
            std::visit(visitor, std::move(expr)->variant());
    }
}


int main(int argc, char *argv[]) {
    // if (argc < 2) error("Please pass a file name!");

    using std::operator""sv;
    const auto canonical_root = std::filesystem::canonical(*argv);;

    bool print_preprocessed = false;
    bool print_tokens       = false;
    bool print_parsed       = false;
    bool print_opt          = false;
    bool run                = true;
    bool repl               = false;

    std::filesystem::path fname;

    // this would leave file name at argv[1]
    for(; argc > 1; --argc, ++argv) {
        if (argv[1] == "-token"sv) print_tokens       = true ;
        else if (argv[1] == "-ast"sv  ) print_parsed       = true ;
        else if (argv[1] == "-pre"sv  ) print_preprocessed = true ;
        else if (argv[1] == "-opt"sv  ) print_opt          = true ;
        else if (argv[1] == "-run"sv  ) run                = false;
        else if (argv[1] == "-repl"sv ) repl               = true ;
        else fname = argv[1];
    }


    if (print_opt) {
        std::clog << std::boolalpha;
        std::clog << "print tokens:        " << print_tokens       << std::endl;
        std::clog << "print parsed:        " << print_parsed       << std::endl;
        std::clog << "print pre-processed: " << print_preprocessed << std::endl;
        std::clog << "run?                 " << run                << std::endl;
        std::clog << "repl?                " << repl               << std::endl;
    }


    if (fname.empty() or repl) {
        REPL(
            std::move(canonical_root),
            print_preprocessed, print_tokens, print_parsed, run
        );
    }
    else {
        runFile(std::move(fname), print_preprocessed, print_tokens, print_parsed, run);
    }
}