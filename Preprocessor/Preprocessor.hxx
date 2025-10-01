#pragma once


#include <print>
#include <cctype>
#include <fstream>
#include <string>
#include <filesystem>
#include <ranges>


bool findAndRemoveImport(std::string& src, size_t& index) {
    using std::operator""sv;

    index = src.find("import");

    if (index == std::string::npos) return false;

    constexpr size_t import_length = "import"sv.length();

    src.erase(index, import_length);

    return true;
}

size_t findSpace(const std::string& src, size_t ind) {
    while(++ind < src.length() and not std::isspace(src[ind]) and src[ind] != ';');

    return ind;
}


std::string preprocess(std::string src, const std::filesystem::path& root) {
    std::filesystem::path mainfile = root;
    mainfile.remove_filename();

    size_t index;

    while (findAndRemoveImport(src, index)) {
        std::filesystem::path filename = mainfile;

        const size_t end_index = findSpace(src, index);
        ++index;
        auto name = src.substr(index, end_index - index);
        src.erase(index, end_index - index + (src[end_index] == ';')); // remove filename

        std::ranges::replace(name, ':', '/');
        filename /= name;
        filename.replace_extension(".pie");

        std::println("file name: {}", filename.string());

        std::ifstream fin{filename};

        if (fin.fail()) {
            std::println(std::cerr, "Imported file: '{}' not found", filename.string());
            exit(1);
        }

        std::string module;
        for (std::string line; std::getline(fin, line); ) module += line + '\n';

        module = preprocess(std::move(module), root);

        src.insert(index, std::move(module));
    }

    return src;
}
