#include "message_store.hpp"

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/stream.hpp>

#include <aws/core/Aws.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>

namespace slabko::s3sink {

MessageStore::MessageStore(size_t max_size)
  : max_size_ { max_size }
  , size_ {}
  , is_running_checkpoint_ { false }
{
  buffer_.reserve(max_size);
}

void MessageStore::BeginCheckpoint(const std::string& name)
{
  spdlog::info("Creating checkpoint {}", name);
  buffer_.resize(0);
  size_ = 0;
  ostream_.reset();
  auto sink = io::back_inserter(buffer_);
  ostream_.push(io::gzip_compressor {});
  ostream_.push(sink);

  checkpoint_name_ = name;
  is_running_checkpoint_ = true;
};

void MessageStore::Push(const char* msg, int64_t length)
{
  ostream_.write(msg, length);
  size_ += length;
}

void MessageStore::Push(const std::string& msg)
{
  ostream_.write(msg.c_str(), msg.length());
}

bool MessageStore::CommitCheckpoint()
{
  spdlog::info("Uploading {}", checkpoint_name_);
  io::close(ostream_);

  io::array device { buffer_.data(), buffer_.size() };
  auto iostream = std::make_shared<io::stream<io::array>>(device);

  Aws::S3::S3Client client;

  Aws::S3::Model::PutObjectRequest request;
  request.WithBucket("slabko-test");
  request.SetKey(checkpoint_name_);
  request.SetBody(iostream);

  auto put_object_outcome = client.PutObject(request);
  if (put_object_outcome.IsSuccess()) {
    spdlog::info("Uploaded {}", checkpoint_name_);
  } else {
    spdlog::error("Failed to upload {}: {}", checkpoint_name_, put_object_outcome.GetError().GetMessage());
    return false;
  }

  checkpoint_name_ = "";
  is_running_checkpoint_ = false;
  return true;
}

}
