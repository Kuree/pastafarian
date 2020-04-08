#include "source.hh"

#include "util.hh"

namespace fsm {

SourceManager::SourceManager(const std::string& src_filename) {
    auto filename = fs::abspath(src_filename);
    src_filenames_ = {filename};
}

SourceManager::SourceManager(const std::vector<std::string>& src_filenames) {
    src_filenames_.reserve(src_filenames.size());
    for (auto const& name : src_filenames) {
        auto filename = fs::abspath(name);
        src_filenames_.emplace_back(filename);
    }
}

SourceManager::SourceManager(const std::vector<std::string>& src_filenames,
                             const std::vector<std::string>& src_include_dirs)
    : SourceManager(src_filenames) {
    src_include_dirs_.reserve(src_include_dirs.size());
    for (auto const &name: src_include_dirs) {
        auto filename = fs::abspath(name);
        src_include_dirs_.emplace_back(filename);
    }
}

}  // namespace fsm