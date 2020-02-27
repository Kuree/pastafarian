#include "parser.hh"

#include <fstream>
#include <iostream>
#include <sstream>

#include "fmt/format.h"

using fmt::format;


void Parser::parse(const std::string &filename, const std::string &top) {
    std::vector<std::string> f;
    f.emplace_back(filename);
    parse(f, top);
}

void Parser::parse(const std::vector<std::string> &files, const std::string &top) {

}