#pragma once


#include <print>
#include <cctype>
#include <string>
#include <fstream>
#include <unordered_set>
#include <filesystem>
#include <ranges>


[[nodiscard]] inline std::string readFile(const std::string& fname);


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


std::string preprocess(std::string src, const std::filesystem::path& root) {
    static std::unordered_set<std::string> cache;

    std::filesystem::path mainfile = root;
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

        // const auto path = std::filesystem::canonical(filename);
        auto path = std::filesystem::absolute(filename);

        if (cache.contains(path.string())) continue;

        auto module = readFile(path);

        module = preprocess(std::move(module), std::move(path));
        cache.insert(path.string());

        src.insert(index, std::move(module));
    }

    return src;
}
