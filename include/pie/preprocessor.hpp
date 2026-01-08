#pragma once


#include <print>
#include <cctype>
#include <string>
#include <fstream>
#include <unordered_set>
#include <filesystem>
#include <ranges>
#include <stdexcept>


[[nodiscard]] inline std::string readFile2(const std::string& fname) {
    const std::ifstream fin{fname};

    if (not fin.is_open()) {
        std::println(std::cerr, "{}", "File \"" + fname + " \"not found!");
        throw std::runtime_error{"file '" + fname + "' not found!"};
    }

    std::stringstream ss;
    ss << fin.rdbuf();

    return ss.str();
}


bool findAndRemoveImport(std::string& src, size_t& index) {
    using std::operator""sv;

    index = src.find("import");

    if (index == std::string::npos) return false;

    constexpr size_t import_length = "import"sv.length();

    src.erase(index, import_length);

    return true;
}

size_t findSpace(const std::string& src, size_t ind) {
    while (++ind < src.length() and not std::isspace(src[ind]) and src[ind] != ';');

    return ind;
}


void removeAllBetween(std::string& s, const std::string_view begin, const std::string_view end) {
    for (size_t ind = s.find(begin); ind != std::string::npos; ind = s.find(begin)) {
        const size_t end_ind = s.find(end, ind); // look for end starting from index "ind"
        s.erase(ind, end_ind - ind);
    }
}


std::string removeComments(std::string src) {
    // removing block comments
    // doing that first so that we don't confuse ".:" with ".::"
    removeAllBetween(src, ".::", "::.");

    removeAllBetween(src, ".:", "\n");

    return src;
}

template <bool = false>
std::string preprocess(std::string src, const std::filesystem::path& root = ".");

template <bool REPL = false>
std::string process(std::string src, const std::filesystem::path& root) {
    static std::unordered_set<std::string> cache;

    auto canonical = std::filesystem::canonical(root);

    if constexpr (not REPL) {
        if (cache.contains(canonical.string())) return "";
        cache.insert(canonical.string());
    }


    auto mainfile = root;
    mainfile.remove_filename();

    size_t index;

    while (findAndRemoveImport(src, index)) {
        auto filename = mainfile;

        const auto end_index = findSpace(src, index);

        ++index;
        auto name = src.substr(index, end_index - index);
        src.erase(index, end_index - index + (src[end_index] == ';')); // remove filename
        // std::ranges::replace(name, ':', '/');

        filename /= name;

        filename.replace_extension(".pie");

        auto path = std::filesystem::canonical(filename);
        // auto path = std::filesystem::absolute(filename);

        auto module = readFile2(path);

        module = preprocess(std::move(module), std::move(path));

        src.insert(index, std::move(module));
    }

    return src;
}


template <bool REPL>
std::string preprocess(std::string src, const std::filesystem::path& root) {
    return process<REPL>(removeComments(std::move(src)), root);
}
