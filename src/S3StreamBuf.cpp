//          Copyright Julianus Pfeuffer 2024.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "genfile/bgen/S3StreamBuf.hpp"

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace genfile {
namespace bgen {

// ─── S3 URI parsing ─────────────────────────────────────────────────────────

bool is_s3_uri(std::string const& filename) {
    return filename.size() > 5 && filename.substr(0, 5) == "s3://";
}

bool parse_s3_uri(std::string const& uri, std::string& bucket, std::string& key) {
    if (!is_s3_uri(uri)) {
        return false;
    }
    // s3://bucket/key
    std::string remainder = uri.substr(5);
    auto slash_pos = remainder.find('/');
    if (slash_pos == std::string::npos || slash_pos == 0) {
        return false;
    }
    bucket = remainder.substr(0, slash_pos);
    key = remainder.substr(slash_pos + 1);
    if (key.empty()) {
        return false;
    }
    return true;
}

// ─── AWS SDK lifecycle (init once) ──────────────────────────────────────────

namespace {
    struct AwsInitGuard {
        AwsInitGuard() {
            Aws::SDKOptions options;
            Aws::InitAPI(options);
        }
        ~AwsInitGuard() {
            Aws::SDKOptions options;
            Aws::ShutdownAPI(options);
        }
    };

    AwsInitGuard& ensure_aws_initialized() {
        static AwsInitGuard guard;
        return guard;
    }
} // anonymous namespace

// ─── S3StreamBuf implementation ─────────────────────────────────────────────

struct S3StreamBuf::Impl {
    std::shared_ptr<Aws::S3::S3Client> client;
};

S3StreamBuf::S3StreamBuf(
    std::string const& bucket,
    std::string const& key,
    std::string const& region,
    std::size_t block_size
)
    : m_impl(std::make_unique<Impl>())
    , m_bucket(bucket)
    , m_key(key)
    , m_block_size(block_size)
    , m_object_size(-1)
    , m_position(0)
    , m_buffer_block_index(-1)
{
    ensure_aws_initialized();

    Aws::Client::ClientConfiguration config;
    if (!region.empty()) {
        config.region = region;
    } else {
        // Respect AWS_DEFAULT_REGION / AWS_REGION env vars
        const char* env_region = std::getenv("AWS_DEFAULT_REGION");
        if (!env_region) env_region = std::getenv("AWS_REGION");
        if (env_region && env_region[0] != '\0') {
            config.region = env_region;
        }
    }

    // Support AWS_ENDPOINT_URL (e.g. for LocalStack or compatible services)
    const char* endpoint_url = std::getenv("AWS_ENDPOINT_URL");
    bool path_style = false;
    if (endpoint_url && endpoint_url[0] != '\0') {
        config.endpointOverride = endpoint_url;
        path_style = true;  // custom endpoints need path-style (not virtual-hosted) addressing
    }

    // Support AWS_NO_SIGN_REQUEST=1 for public / anonymous buckets.
    // Wrap empty AWSCredentials in a SimpleAWSCredentialsProvider (the S3Client
    // constructor takes a CredentialsProvider, not a raw AWSCredentials object).
    const char* no_sign = std::getenv("AWS_NO_SIGN_REQUEST");
    if (no_sign && (std::string(no_sign) == "1" || std::string(no_sign) == "true")) {
        auto anon_provider = std::make_shared<Aws::Auth::SimpleAWSCredentialsProvider>(
            Aws::Auth::AWSCredentials("", ""));
        m_impl->client = std::make_shared<Aws::S3::S3Client>(
            anon_provider, config,
            Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::RequestDependent,
            !path_style);
    } else {
        m_impl->client = std::make_shared<Aws::S3::S3Client>(
            config,
            Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::RequestDependent,
            !path_style);
    }

    // HEAD request to get object size
    Aws::S3::Model::HeadObjectRequest head_request;
    head_request.SetBucket(m_bucket);
    head_request.SetKey(m_key);

    auto outcome = m_impl->client->HeadObject(head_request);
    if (!outcome.IsSuccess()) {
        throw std::runtime_error(
            "S3StreamBuf: failed to HEAD s3://" + m_bucket + "/" + m_key +
            " — " + outcome.GetError().GetMessage());
    }
    m_object_size = outcome.GetResult().GetContentLength();
}

S3StreamBuf::~S3StreamBuf() = default;

std::int64_t S3StreamBuf::object_size() const {
    return m_object_size;
}

std::int64_t S3StreamBuf::current_position() const {
    return m_position;
}

void S3StreamBuf::fetch_block(std::int64_t block_index) {
    if (block_index == m_buffer_block_index) {
        return; // already loaded
    }

    std::int64_t start = block_index * static_cast<std::int64_t>(m_block_size);
    std::int64_t end = std::min(start + static_cast<std::int64_t>(m_block_size) - 1,
                                m_object_size - 1);

    // Build range string: "bytes=start-end"
    std::ostringstream range_ss;
    range_ss << "bytes=" << start << "-" << end;

    Aws::S3::Model::GetObjectRequest request;
    request.SetBucket(m_bucket);
    request.SetKey(m_key);
    request.SetRange(range_ss.str());

    auto outcome = m_impl->client->GetObject(request);
    if (!outcome.IsSuccess()) {
        throw std::runtime_error(
            "S3StreamBuf: failed to GET range " + range_ss.str() +
            " from s3://" + m_bucket + "/" + m_key +
            " — " + outcome.GetError().GetMessage());
    }

    auto& body = outcome.GetResult().GetBody();
    std::int64_t bytes_expected = end - start + 1;
    m_buffer.resize(static_cast<std::size_t>(bytes_expected));
    body.read(m_buffer.data(), bytes_expected);
    std::streamsize bytes_read = body.gcount();
    m_buffer.resize(static_cast<std::size_t>(bytes_read));

    m_buffer_block_index = block_index;
}

S3StreamBuf::int_type S3StreamBuf::underflow() {
    if (m_position >= m_object_size) {
        return traits_type::eof();
    }

    std::int64_t block_index = m_position / static_cast<std::int64_t>(m_block_size);
    fetch_block(block_index);

    std::int64_t offset_in_block = m_position - block_index * static_cast<std::int64_t>(m_block_size);
    std::int64_t available = static_cast<std::int64_t>(m_buffer.size()) - offset_in_block;

    if (available <= 0) {
        return traits_type::eof();
    }

    // Set up the get area
    char* begin = m_buffer.data() + offset_in_block;
    char* end = m_buffer.data() + m_buffer.size();
    setg(begin, begin, end);

    return traits_type::to_int_type(*gptr());
}

S3StreamBuf::pos_type S3StreamBuf::seekoff(
    off_type off, std::ios_base::seekdir dir,
    std::ios_base::openmode which)
{
    if (!(which & std::ios_base::in)) {
        return pos_type(off_type(-1));
    }

    std::int64_t new_pos;
    switch (dir) {
        case std::ios_base::beg:
            new_pos = off;
            break;
        case std::ios_base::cur:
            // Current position = position + (gptr - eback) offset within buffer
            new_pos = m_position + (gptr() - eback()) + off;
            break;
        case std::ios_base::end:
            new_pos = m_object_size + off;
            break;
        default:
            return pos_type(off_type(-1));
    }

    if (new_pos < 0 || new_pos > m_object_size) {
        return pos_type(off_type(-1));
    }

    m_position = new_pos;
    // Invalidate the get area so next read triggers underflow
    setg(nullptr, nullptr, nullptr);

    return pos_type(new_pos);
}

S3StreamBuf::pos_type S3StreamBuf::seekpos(
    pos_type pos, std::ios_base::openmode which)
{
    return seekoff(off_type(pos), std::ios_base::beg, which);
}

std::streamsize S3StreamBuf::showmanyc() {
    std::int64_t remaining = m_object_size - m_position;
    if (gptr() && gptr() < egptr()) {
        remaining = m_object_size - (m_position + (gptr() - eback()));
    }
    return static_cast<std::streamsize>(std::max<std::int64_t>(0, remaining));
}

// ─── Factory function ───────────────────────────────────────────────────────

std::unique_ptr<std::istream> make_s3_istream(
    std::string const& s3_uri,
    std::string const& region)
{
    std::string bucket, key;
    if (!parse_s3_uri(s3_uri, bucket, key)) {
        throw std::invalid_argument(
            "make_s3_istream: invalid S3 URI: " + s3_uri);
    }

    auto streambuf = std::make_unique<S3StreamBuf>(bucket, key, region);

    // Create istream that owns the streambuf
    auto stream = std::make_unique<std::istream>(streambuf.get());
    // Transfer ownership of streambuf via a custom deleter isn't straightforward;
    // instead we use a small wrapper class.
    struct S3IStream : public std::istream {
        std::unique_ptr<S3StreamBuf> buf;
        S3IStream(std::unique_ptr<S3StreamBuf> b)
            : std::istream(b.get()), buf(std::move(b)) {}
    };

    return std::make_unique<S3IStream>(std::move(streambuf));
}

} // namespace bgen
} // namespace genfile
