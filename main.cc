#include <string>
#include <string_view>
#include <filesystem>


#include "Utils/CLI.hxx"


int main(int argc, char *argv[]) {
    // if (argc < 2) error("Please pass a file name!");

    using std::operator""sv;
    const auto canonical_root = std::filesystem::canonical(*argv);

    bool print_preprocessed = false;
    bool print_tokens       = false;
    bool print_parsed       = false;
    bool print_help         = false;
    bool run                = true;
    bool repl               = false;


    std::filesystem::path fname;

    // this would leave file name at argv[1]
    for(; argc > 1; --argc, ++argv) {
             if (argv[1] == "-token"sv) print_tokens       = true ;
        else if (argv[1] == "-ast"sv  ) print_parsed       = true ;
        else if (argv[1] == "-pre"sv  ) print_preprocessed = true ;
        else if (argv[1] == "-help"sv ) print_help         = true ;
        else if (argv[1] == "-run"sv  ) run                = false;
        else if (argv[1] == "-repl"sv ) repl               = true ;
        else fname = argv[1];
    }



    if (print_help) {
        pie::cli::help();
        return 0;
    }


    if (fname.empty() or repl) {
        pie::cli::REPL(
            std::move(canonical_root),
            print_preprocessed, print_tokens, print_parsed, run
        );
    }
    else try {
        pie::cli::runFile(std::move(fname), print_preprocessed, print_tokens, print_parsed, run);
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}