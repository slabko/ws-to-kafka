#include "../pch.h"

#include <boost/iostreams/filtering_stream.hpp>

namespace slabko::s3sink {

namespace io = boost::iostreams;

class MessageStore final {
public:
  explicit MessageStore(size_t max_size);
  void BeginCheckpoint(const std::string& name);
  [[nodiscard]] bool IsRunningCheckpoint() const { return is_running_checkpoint_; };
  void Push(const char* msg, int64_t length);
  void Push(const std::string& msg);
  void CommitCheckpoint();
  [[nodiscard]] size_t Size() const { return size_; }
  [[nodiscard]] size_t CompressedSize() const { return buffer_.size(); }

private:
  size_t max_size_;
  size_t size_;
  bool is_running_checkpoint_;
  std::vector<char> buffer_;

  std::string checkpoint_name_;

  io::filtering_ostream ostream_;
};
}
