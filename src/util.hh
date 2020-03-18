#ifndef PASTAFARIAN_UTIL_HH
#define PASTAFARIAN_UTIL_HH

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fsm {

std::string parse_verilog(const std::string &filename);
std::string parse_verilog(const std::vector<std::string> &filenames);
std::string parse_verilog(const std::vector<std::string> &filenames,
                          const std::vector<std::string> &include_dirs);

static void assert(bool condition, const std::string &what = "");

// this is from kratos
namespace fs {
std::string join(const std::string &path1, const std::string &path2);
std::string which(const std::string &name);
bool exists(const std::string &filename);
bool remove(const std::string &filename);
std::string temp_directory_path();
std::string get_ext(const std::string &filename);
std::string abspath(const std::string &filename);
std::string basename(const std::string &filename);
char separator();
}  // namespace fs

namespace string {
void trim(std::string &str);
std::vector<std::string> get_tokens(std::string_view line, const std::string &delimiter);
template <typename Iter>
std::string static join(Iter begin, Iter end, const std::string &sep) {
    std::stringstream stream;
    for (auto it = begin; it != end; it++) {
        if (it != begin) stream << sep;
        stream << *it;
    }
    return stream.str();
}
}  // namespace string
}  // namespace fsm

#endif  // PASTAFARIAN_UTIL_HH
