#ifndef PASTAFARIAN_UTIL_HH
#define PASTAFARIAN_UTIL_HH

#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "source.hh"

namespace fsm {

void assert_(bool condition, const std::string &what = "");

uint32_t get_num_cpus();
void set_num_cpus(int num_cpu);

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
std::string resolve(const std::string &filename, const std::string &base);
std::string dirname(const std::string &filename);
char separator();
std::string getcwd();
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

namespace json {
class JSONWriter {
public:
    template <typename T>
    JSONWriter &write(std::string_view name, T value) {
        end();
        indent();
        stream_ << "\"" << name << "\": ";
        return write_(value);
    }

    template <typename T>
    JSONWriter &write(T value) {
        end();
        indent();
        return write_(value);
    }

    JSONWriter &start_array(std::string_view name);
    JSONWriter &start_array();
    JSONWriter &end_array();

    JSONWriter &start_object(std::string_view name);
    JSONWriter &start_object();
    JSONWriter &end_object();

    [[nodiscard]] std::string str() const;

private:
    uint32_t indent_ = 0;
    bool end_ = false;
    std::stringstream stream_;

    inline void indent() {
        for (uint32_t i = 0; i < indent_; i++) stream_ << ' ';
    }

    template <typename T>
    JSONWriter &write_(T value) {
        if constexpr (std::is_same_v<std::string, T>)
            stream_ << "\"" << value << "\"";
        else
            stream_ << value;
        end_ = true;
        return *this;
    }

    void end();
};
}  // namespace json

}  // namespace fsm

#endif  // PASTAFARIAN_UTIL_HH
