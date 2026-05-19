//          Copyright Julianus Pfeuffer 2024.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef GENFILE_BGEN_S3STREAMBUF_HPP
#define GENFILE_BGEN_S3STREAMBUF_HPP

#include <streambuf>
#include <istream>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace genfile {
namespace bgen {

/// Parse an S3 URI of the form s3://bucket/key into its components.
/// Returns true if the URI was successfully parsed.
bool parse_s3_uri(std::string const& uri, std::string& bucket, std::string& key);

/// Returns true if the given filename looks like an S3 URI (starts with "s3://").
bool is_s3_uri(std::string const& filename);

/// A std::streambuf implementation that reads from AWS S3 using range requests.
/// Supports seeking (seekg) and buffered block-based reads for efficiency.
class S3StreamBuf : public std::streambuf {
public:
    /// Construct an S3StreamBuf for the given bucket and key.
    /// block_size controls the granularity of range GET requests (default 1 MB).
    S3StreamBuf(std::string const& bucket, std::string const& key,
                std::string const& region = "",
                std::size_t block_size = 1024 * 1024);

    ~S3StreamBuf() override;

    /// Get the total size of the S3 object in bytes.
    std::int64_t object_size() const;

protected:
    // std::streambuf overrides
    int_type underflow() override;
    pos_type seekoff(off_type off, std::ios_base::seekdir dir,
                     std::ios_base::openmode which = std::ios_base::in) override;
    pos_type seekpos(pos_type pos,
                     std::ios_base::openmode which = std::ios_base::in) override;
    std::streamsize showmanyc() override;

private:
    void fetch_block(std::int64_t block_index);
    std::int64_t current_position() const;

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::string m_bucket;
    std::string m_key;
    std::size_t m_block_size;
    std::int64_t m_object_size;
    std::int64_t m_position; // logical position in the stream

    // Buffer for the currently loaded block
    std::vector<char> m_buffer;
    std::int64_t m_buffer_block_index; // which block is loaded (-1 = none)
};

/// Create an std::istream that reads from an S3 URI.
/// The URI must be of the form s3://bucket/key.
/// Optionally specify the AWS region (otherwise uses SDK defaults/env).
std::unique_ptr<std::istream> make_s3_istream(
    std::string const& s3_uri,
    std::string const& region = "");

} // namespace bgen
} // namespace genfile

#endif
