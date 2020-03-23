#include "util.hh"

#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>

using fmt::format;

// we only support Linux
#define INCLUDE_FILESYSTEM

namespace fsm {

std::string parse_verilog(const std::string &filename) {
    std::vector<std::string> name = {filename};
    return parse_verilog(name);
}
std::string parse_verilog(const std::vector<std::string> &filenames) {
    return parse_verilog(filenames, {});
}

std::string parse_verilog(const std::vector<std::string> &filenames,
                          const std::vector<std::string> &include_dirs) {
    // need to run slang to get the ast json
    // make sure slang exists
    // if SLANG is set in the env
    std::string slang;
    auto slang_char = std::getenv("SLANG");
    if (slang_char) {
        slang = std::string(slang_char);
    }
    if (slang.empty()) {
        slang = fs::which("slang");
        if (slang.empty()) {
            slang = fs::which("slang-driver");
        }
    }
    if (slang.empty()) throw std::runtime_error("Unable to find slang driver");
    // prepare for the slang arguments
    std::vector<std::string> args = {slang};
    args.reserve(1 + filenames.size() + 1 + include_dirs.size() + 2);
    // all the files
    args.insert(args.end(), filenames.begin(), filenames.end());
    // if we have include dirs
    if (!include_dirs.empty()) {
        args.emplace_back("-I");
        args.insert(args.end(), include_dirs.begin(), include_dirs.end());
    }
    // json output
    // get a random filename
    std::string temp_dir = fs::temp_directory_path();
    auto temp_filename = fs::join(temp_dir, "output.json");
    args.emplace_back("--ast-json");
    args.emplace_back(temp_filename);
    auto command = string::join(args.begin(), args.end(), " ");
    auto ret = std::system(command.c_str());
    if (ret) {
        throw std::runtime_error(
            ::format("Unable to parse {0}", string::join(filenames.begin(), filenames.end(), " ")));
    }
    return temp_filename;
}

void assert_(bool condition, const std::string &what) {
    if (!condition) {
        throw std::runtime_error("Assert failed. Reason: " + (what.empty() ? "null" : what));
    }
}

static int _num_cpu = -1;

uint32_t get_num_cpus() {
    if (_num_cpu < 0) {
        // compute the number of CPUs being used
        uint32_t num_cpus = std::thread::hardware_concurrency();
        _num_cpu = static_cast<int>(std::max(1u, num_cpus / 2));
    }
    return static_cast<uint32_t>(_num_cpu);
}
void set_num_cpus(int num_cpu) { _num_cpu = num_cpu; }

namespace fs {
std::string which(const std::string &name) {
    // windows is more picky
    std::string env_path;
#ifdef _WIN32
    char *path_var;
    size_t len;
    auto err = _dupenv_s(&path_var, &len, "PATH");
    if (err) {
        env_path = "";
    }
    env_path = std::string(path_var);
    free(path_var);
    path_var = nullptr;
#else
    env_path = std::getenv("PATH");
#endif
    // tokenize it base on either : or ;
    auto tokens = string::get_tokens(env_path, ";:");
    for (auto const &dir : tokens) {
        auto new_path = fs::join(dir, name);
        if (exists(new_path)) {
            return new_path;
        }
    }
    return "";
}

bool exists(const std::string &filename) {
#if defined(INCLUDE_FILESYSTEM)
    namespace fs = std::filesystem;
    return fs::exists(filename);
#else
    std::ifstream in(filename);
    return in.good();
#endif
}

std::string join(const std::string &path1, const std::string &path2) {
#if defined(INCLUDE_FILESYSTEM)
    namespace fs = std::filesystem;
    fs::path p1 = path1;
    fs::path p2 = path2;
    return p1 / p2;
#else
    return path1 + "/" + path2;
#endif
}

bool remove(const std::string &filename) {
#if defined(INCLUDE_FILESYSTEM)
    namespace fs = std::filesystem;
    return fs::remove(filename);
#else
    return std::remove(filename.c_str());
#endif
}

std::string temp_directory_path() {
#if defined(INCLUDE_FILESYSTEM)
    namespace fs = std::filesystem;
    return fs::temp_directory_path();
#else
    return "/tmp";
#endif
}

std::string get_ext(const std::string &filename) {
#if defined(INCLUDE_FILESYSTEM)
    std::filesystem::path path(filename);
    return path.extension().string();
#else
    auto idx = filename.rfind('.');
    if (idx != std::string::npos)
        return filename.substr(idx);
    else
        return "";
#endif
}

std::string abspath(const std::string &filename) {
#if defined(INCLUDE_FILESYSTEM)
    return std::filesystem::absolute(filename);
#else
#if defined(NO_FS_HAS_REALPATH)
    auto path = realpath(filename.c_str(), nullptr);
#else
    auto path = _fullpath(NULL, filename.c_str(), 120);
#endif
    return std::string(path);
#endif
}

std::string basename(const std::string &filename) {
#if defined(INCLUDE_FILESYSTEM)
    std::filesystem::path path(filename);
    return path.filename();
#else
    auto tokens = string::get_tokens(filename, "/\\");
    return tokens.back();
#endif
}

char separator() {
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}
}  // namespace fs

namespace string {
// trim function copied from https://stackoverflow.com/a/217605
// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(),
            s.end());
}

// trim from both ends (in place)
void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

std::vector<std::string> get_tokens(std::string_view line, const std::string &delimiter) {
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    std::string token;
    // copied from https://stackoverflow.com/a/7621814
    while ((pos = line.find_first_of(delimiter, prev)) != std::string::npos) {
        if (pos > prev) {
            tokens.emplace_back(line.substr(prev, pos - prev));
        }
        prev = pos + 1;
    }
    if (prev < line.length()) tokens.emplace_back(line.substr(prev, std::string::npos));
    // remove empty ones
    std::vector<std::string> result;
    result.reserve(tokens.size());
    for (auto const &t : tokens)
        if (!t.empty()) result.emplace_back(t);
    return result;
}

}  // namespace string

namespace json {
JSONWriter &JSONWriter::start_array(std::string_view name) {
    end();
    indent();
    stream_ << "\"" << name << "\": [" << std::endl;
    indent_ += 2;
    return *this;
}

JSONWriter &JSONWriter::start_array() {
    end();
    indent();
    stream_ << '[' << std::endl;
    indent_ += 2;
    return *this;
}

JSONWriter &JSONWriter::end_array() {
    // no , at the end of an array
    stream_ << std::endl;
    indent_ -= 2;
    indent();
    stream_ << "]";
    end_ = true;
    return *this;
}

JSONWriter &JSONWriter::start_object(std::string_view name) {
    end();
    indent();
    stream_ << "\"" << name << "\": \" {" << std::endl;
    indent_ += 2;
    return *this;
}

JSONWriter & JSONWriter::start_object() {
    end();
    indent();
    stream_ << '{' << std::endl;
    indent_ += 2;
    return *this;
}

JSONWriter &JSONWriter::end_object() {
    stream_ << std::endl;
    // no , at the end of an object
    indent_ -= 2;
    indent();
    stream_ << "}";
    end_ = true;
    return *this;
}

std::string JSONWriter::str() const {
    assert_(indent_ == 0, "incorrect JSON hierarchy");
    return stream_.str();
}

void JSONWriter::end() {
    if (end_) {
        stream_ << "," << std::endl;
        end_ = false;
    }
}

}
}  // namespace fsm