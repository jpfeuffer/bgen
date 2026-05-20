// S3 integration test for the bgen S3 streaming layer.
//
// Environment variables (all optional; test is skipped when absent):
//
//   BGEN_S3_TEST_URI
//       s3://bucket/path/to/file.bgen  — accessed with default credential chain
//       (AWS_ACCESS_KEY_ID / AWS_SECRET_ACCESS_KEY / instance profile / etc.)
//
//   BGEN_S3_PUBLIC_TEST_URI
//       s3://bucket/path/to/file.bgen  — accessed with AWS_NO_SIGN_REQUEST=1
//       Must be in a bucket / with an object ACL that allows anonymous reads.
//
// Expected BGEN properties (can be overridden via env):
//   BGEN_S3_EXPECTED_VARIANTS   number of variants (default: 199)
//   BGEN_S3_EXPECTED_SAMPLES    number of samples  (default: 500)
//
// To run:
//   BGEN_S3_TEST_URI=s3://mybucket/example.16bits.bgen \
//       ctest --test-dir build -R test_s3_bgen -V
//
// For public-bucket anonymous access:
//   BGEN_S3_PUBLIC_TEST_URI=s3://mybucket/example.16bits.bgen \
//       ctest --test-dir build -R test_s3_bgen -V

#define CATCH_CONFIG_NO_POSIX_SIGNALS
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "genfile/bgen/S3StreamBuf.hpp"
#include "genfile/bgen/bgen.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// ─── Helpers ─────────────────────────────────────────────────────────────────

namespace {

// Holds what check_bgen_header returns.
struct BgenHeader {
    genfile::bgen::Context ctx;
    std::streamoff data_start; // = 4 + file_offset, byte position of first variant
};

// Read and verify a BGEN header from the given stream.
BgenHeader check_bgen_header(std::istream& stream,
                              uint32_t expected_variants,
                              uint32_t expected_samples)
{
    uint32_t offset = 0;
    genfile::bgen::read_offset(stream, &offset);
    REQUIRE(offset > 0);

    genfile::bgen::Context ctx;
    genfile::bgen::read_header_block(stream, &ctx);

    REQUIRE(ctx.number_of_variants == expected_variants);
    REQUIRE(ctx.number_of_samples  == expected_samples);
    REQUIRE((ctx.flags & 0x3u) != 0); // layout bits set

    return { ctx, static_cast<std::streamoff>(4 + offset) };
}

// Helper: read one variant's identifying data + skip genotype block.
void skip_one_variant(std::istream& stream, genfile::bgen::Context const& ctx)
{
    std::string snpid, rsid, chrom, allele0, allele1;
    uint32_t position = 0;

    bool ok = genfile::bgen::read_snp_identifying_data(
        stream, ctx,
        &snpid, &rsid, &chrom, &position,
        &allele0, &allele1
    );
    REQUIRE(ok);
    REQUIRE(!chrom.empty());
    REQUIRE(position > 0);

    genfile::bgen::ignore_genotype_data_block(stream, ctx);
}

// Parse uint32 from an env var (returns default_val if absent/invalid).
uint32_t env_uint32(const char* name, uint32_t default_val)
{
    const char* v = std::getenv(name);
    if (!v || v[0] == '\0') return default_val;
    try { return static_cast<uint32_t>(std::stoul(v)); }
    catch (...) { return default_val; }
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — authenticated (default credential chain)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("S3 authenticated read: BGEN header and first variant", "[s3][auth]")
{
    const char* uri_cstr = std::getenv("BGEN_S3_TEST_URI");
    if (!uri_cstr || uri_cstr[0] == '\0') {
        WARN("Skipped — set BGEN_S3_TEST_URI to enable this test");
        return;
    }
    std::string uri(uri_cstr);
    uint32_t exp_variants = env_uint32("BGEN_S3_EXPECTED_VARIANTS", 199);
    uint32_t exp_samples  = env_uint32("BGEN_S3_EXPECTED_SAMPLES",  500);

    INFO("URI: " << uri);

    std::unique_ptr<std::istream> stream;
    REQUIRE_NOTHROW(stream = genfile::bgen::make_s3_istream(uri));
    REQUIRE(stream != nullptr);
    REQUIRE(stream->good());

    auto hdr = check_bgen_header(*stream, exp_variants, exp_samples);

    // Seek to first variant and read it
    stream->seekg(hdr.data_start);
    skip_one_variant(*stream, hdr.ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — anonymous / public bucket (AWS_NO_SIGN_REQUEST=1)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("S3 anonymous read: BGEN header and first variant", "[s3][anon]")
{
    const char* uri_cstr = std::getenv("BGEN_S3_PUBLIC_TEST_URI");
    if (!uri_cstr || uri_cstr[0] == '\0') {
        WARN("Skipped — set BGEN_S3_PUBLIC_TEST_URI to enable this test");
        return;
    }
    std::string uri(uri_cstr);
    uint32_t exp_variants = env_uint32("BGEN_S3_EXPECTED_VARIANTS", 199);
    uint32_t exp_samples  = env_uint32("BGEN_S3_EXPECTED_SAMPLES",  500);

    INFO("URI: " << uri);

    // Enable anonymous (unsigned) requests for this test
#ifndef _WIN32
    ::setenv("AWS_NO_SIGN_REQUEST", "1", 1);
#endif

    std::unique_ptr<std::istream> stream;
    REQUIRE_NOTHROW(stream = genfile::bgen::make_s3_istream(uri));
    REQUIRE(stream != nullptr);
    REQUIRE(stream->good());

    auto hdr = check_bgen_header(*stream, exp_variants, exp_samples);

    stream->seekg(hdr.data_start);
    skip_one_variant(*stream, hdr.ctx);

#ifndef _WIN32
    ::unsetenv("AWS_NO_SIGN_REQUEST");
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — seeking / random access
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("S3 authenticated read: seeking works correctly", "[s3][auth][seek]")
{
    const char* uri_cstr = std::getenv("BGEN_S3_TEST_URI");
    if (!uri_cstr || uri_cstr[0] == '\0') {
        WARN("Skipped — set BGEN_S3_TEST_URI to enable this test");
        return;
    }

    std::unique_ptr<std::istream> stream;
    REQUIRE_NOTHROW(stream = genfile::bgen::make_s3_istream(std::string(uri_cstr)));

    REQUIRE(stream->tellg() == std::streampos(0));

    // Read 4 bytes (the offset field)
    char buf[4] = {};
    stream->read(buf, 4);
    REQUIRE(stream->gcount() == 4);
    REQUIRE(stream->tellg() == std::streampos(4));

    // Seek back to 0 and re-read — must get same bytes
    stream->seekg(0, std::ios_base::beg);
    REQUIRE(stream->tellg() == std::streampos(0));

    char buf2[4] = {};
    stream->read(buf2, 4);
    REQUIRE(stream->gcount() == 4);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(static_cast<unsigned char>(buf[i]) ==
                static_cast<unsigned char>(buf2[i]));
    }

    // Seek to end and verify size is non-zero
    stream->seekg(0, std::ios_base::end);
    REQUIRE(stream->tellg() > std::streampos(0));
}
