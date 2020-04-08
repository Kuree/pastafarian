#ifndef PASTAFARIAN_SOURCE_HH
#define PASTAFARIAN_SOURCE_HH

#include <string>
#include <vector>

namespace fsm {

class SourceManager {
public:
    SourceManager() = default;
    explicit SourceManager(const std::string &src_filename);
    explicit SourceManager(const std::vector<std::string> &src_filenames);
    SourceManager(const std::vector<std::string> &src_filenames,
                  const std::vector<std::string> &src_include_dirs);

    void set_json_filename(const std::string &value) { json_filename_ = value; }
    [[nodiscard]] const std::string &json_filename() const { return json_filename_; }
    [[nodiscard]] const std::vector<std::string> &src_filenames() const { return src_filenames_; }
    [[nodiscard]] const std::vector<std::string> &src_include_dirs() const {
        return src_include_dirs_;
    }

private:
    std::string json_filename_;
    std::vector<std::string> src_filenames_;
    std::vector<std::string> src_include_dirs_;
};

}  // namespace fsm
#endif  // PASTAFARIAN_SOURCE_HH