#include "source.hh"

#include <fstream>
#include <unordered_set>

#include "util.hh"

namespace fsm {

SourceManager::SourceManager(const std::string& src_filename) {
    auto filename = fs::abspath(src_filename);
    if (is_file_lit(filename)) {
        // read out the file extensions
        read_file_list(filename);
    } else {
        src_filenames_ = {filename};
    }
}

SourceManager::SourceManager(const std::vector<std::string>& src_filenames) {
    if (src_filenames.size() == 1 && is_file_lit(src_filenames[0])) {
        read_file_list(src_filenames[0]);
    } else {
        src_filenames_.reserve(src_filenames.size());
        for (auto const& name : src_filenames) {
            auto filename = fs::abspath(name);
            src_filenames_.emplace_back(filename);
        }
    }
}

SourceManager::SourceManager(const std::vector<std::string>& src_filenames,
                             const std::vector<std::string>& src_include_dirs)
    : SourceManager(src_filenames) {
    src_include_dirs_.reserve(src_include_dirs.size());
    for (auto const& name : src_include_dirs) {
        auto filename = fs::abspath(name);
        src_include_dirs_.emplace_back(filename);
    }
}

void SourceManager::read_file_list(const std::string& filename) {
    auto dir_name = fs::dirname(filename);
    std::ifstream file(filename);
    for (std::string line; std::getline(file, line);) {
        auto p = fs::resolve(line, dir_name);
        src_filenames_.emplace_back(p);
    }
}

bool SourceManager::is_file_lit(const std::string &filename) {
    // it really depends on the file extension
    const static std::unordered_set<std::string> file_list_extensions = {".list", ".filelist",
                                                                         ".txt"};
    auto ext = fs::get_ext(filename);
    return file_list_extensions.find(ext) != file_list_extensions.end();
}

}  // namespace fsm