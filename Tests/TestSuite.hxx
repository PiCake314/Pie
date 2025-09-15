#pragma once

#include <iostream>
#include <string>
#include <cstdio>
#include <unistd.h>


#include "../Lexer/Lexer.hxx"
#include "../Parser/Parser.hxx"
#include "../Interp/Interpreter.hxx"


struct Capture {
    int oldfd{-1}; FILE* tmp{nullptr}; std::string s; bool stopped{false};
    Capture() {
        tmp = std::tmpfile();

        oldfd = dup(STDOUT_FILENO);

        dup2(fileno(tmp), STDOUT_FILENO);
    }

    std::string stop() {
        if (stopped) return s;
        std::cout.flush(); std::fflush(stdout);

        dup2(oldfd, STDOUT_FILENO); close(oldfd);

        std::rewind(tmp);

        char buf[4096]; size_t n;
        while ((n = std::fread(buf,1,sizeof buf,tmp)) > 0) s.append(buf, n);

        std::fclose(tmp);
        stopped = true;

        clean();
        return s;
    }

    void clean () {
        s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
        s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());


        for (size_t i=0;i<s.size();) {
            if (s[i] == 0x1B && i+1 < s.size() && s[i+1] == '[') {
                size_t j = i + 2;
                while (j < s.size() && (std::isdigit((unsigned char)s[j]) || s[j] == ';')) ++j;

                if (j<s.size()) s.erase(i, j+1-i); else break;
            }
            else ++i;
        }

        while (!s.empty() && (s.back()=='\n' || s.back()==' ')) s.pop_back();
    };

    ~Capture(){ if (!stopped) stop(); }

    const std::string& text() const { return s; }
};



std::string run(const char* src) {
    Capture c{};

    const Tokens v = lex(src);

    if (v.empty()) return;

    Parser p{v};

    const auto [exprs, ops] = p.parse();


    Visitor visitor{ops};
    for (const auto& expr : exprs)
        std::visit(visitor, expr->variant());

    return c.stop();
}

