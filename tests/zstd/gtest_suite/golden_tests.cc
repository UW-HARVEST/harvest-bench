// Data-driven tests over the upstream golden file sets (upstream/golden-*).
// Upstream consumes these through CLI tests (playTests.sh / cli-tests); here
// they are exercised through the library API directly:
//   golden-decompression/*.zst        -> must decompress successfully
//   golden-decompression-errors/*.zst -> decompression must report an error
//   golden-compression/*              -> compress/decompress round trip at
//                                        several levels must be byte-identical
//   golden-dictionaries + dict-files  -> dictionary load + round trip
//
// The data directory is baked in at configure time via ZSTD_GTEST_DATA_DIR.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

extern "C" {
#include "zstd.h"
}

namespace {

namespace fs = std::filesystem;

std::string DataDir() { return ZSTD_GTEST_DATA_DIR; }

std::vector<unsigned char> ReadFile(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  EXPECT_TRUE(f.good()) << "cannot open " << p;
  return std::vector<unsigned char>(std::istreambuf_iterator<char>(f),
                                    std::istreambuf_iterator<char>());
}

// Sanitize a filename into a gtest parameter name (alphanumerics only).
std::string ParamName(const std::string& stem) {
  std::string out;
  for (char c : stem) {
    if (std::isalnum(static_cast<unsigned char>(c))) out += c;
  }
  return out.empty() ? "file" : out;
}

std::vector<std::string> ListDir(const std::string& sub) {
  std::vector<std::string> names;
  for (const auto& e : fs::directory_iterator(fs::path(DataDir()) / sub)) {
    const std::string name = e.path().filename().string();
    if (e.is_regular_file() && !name.empty() && name[0] != '.') {
      names.push_back(name);
    }
  }
  std::sort(names.begin(), names.end());
  return names;
}

// Non-zero sentinel for "this input is not a decodable frame at all",
// distinct from the error codes the library itself reports.
constexpr std::size_t kNotAFrame = static_cast<std::size_t>(-1);

// Streaming decompression; returns the ZSTD error code of the first failing
// call (0 = whole input consumed successfully) and appends output to *out.
size_t StreamDecompress(const std::vector<unsigned char>& in,
                        std::vector<unsigned char>* out) {
  ZSTD_DCtx* const dctx = ZSTD_createDCtx();
  EXPECT_NE(dctx, nullptr);
  std::vector<unsigned char> chunk(1 << 17);
  ZSTD_inBuffer ib = {in.data(), in.size(), 0};
  std::size_t err = 0;
  // Empty input is not a valid frame; report it as an error for the
  // error-set tests (no golden file is empty in the success set).
  if (in.empty()) err = kNotAFrame;
  while (ib.pos < ib.size) {
    ZSTD_outBuffer ob = {chunk.data(), chunk.size(), 0};
    std::size_t const ret = ZSTD_decompressStream(dctx, &ob, &ib);
    if (out) out->insert(out->end(), chunk.data(), chunk.data() + ob.pos);
    if (ZSTD_isError(ret)) {
      err = ret;
      break;
    }
    if (ret == 0 && ib.pos == ib.size) break;  // frame done, input consumed
    if (ob.pos == 0 && ib.pos == ib.size && ret != 0) {
      err = kNotAFrame;  // truncated frame: input ended mid-frame
      break;
    }
  }
  ZSTD_freeDCtx(dctx);
  return err;
}

// ---------------------------------------------------------------------------

class GoldenDecompression : public testing::TestWithParam<std::string> {};

TEST_P(GoldenDecompression, DecodesCleanly) {
  const std::vector<unsigned char> in =
      ReadFile(fs::path(DataDir()) / "golden-decompression" / GetParam());
  std::vector<unsigned char> out;
  std::size_t const err = StreamDecompress(in, &out);
  EXPECT_EQ(err, 0u) << ZSTD_getErrorName(err);
}

INSTANTIATE_TEST_SUITE_P(
    AllFiles, GoldenDecompression,
    testing::ValuesIn(ListDir("golden-decompression")),
    [](const testing::TestParamInfo<std::string>& info) {
      return ParamName(info.param);
    });

class GoldenDecompressionErrors : public testing::TestWithParam<std::string> {};

TEST_P(GoldenDecompressionErrors, IsRejected) {
  const std::vector<unsigned char> in = ReadFile(
      fs::path(DataDir()) / "golden-decompression-errors" / GetParam());
  std::size_t const err = StreamDecompress(in, nullptr);
  EXPECT_NE(err, 0u) << "malformed frame decoded without error";
}

INSTANTIATE_TEST_SUITE_P(
    AllFiles, GoldenDecompressionErrors,
    testing::ValuesIn(ListDir("golden-decompression-errors")),
    [](const testing::TestParamInfo<std::string>& info) {
      return ParamName(info.param);
    });

class GoldenCompression : public testing::TestWithParam<std::string> {};

TEST_P(GoldenCompression, RoundTrips) {
  const std::vector<unsigned char> src =
      ReadFile(fs::path(DataDir()) / "golden-compression" / GetParam());
  for (int level : {1, 3, 19}) {
    SCOPED_TRACE(testing::Message() << "level " << level);
    std::vector<unsigned char> dst(ZSTD_compressBound(src.size()));
    std::size_t const csize = ZSTD_compress(dst.data(), dst.size(), src.data(),
                                       src.size(), level);
    ASSERT_FALSE(ZSTD_isError(csize)) << ZSTD_getErrorName(csize);
    std::vector<unsigned char> rt(src.size() + 1);
    std::size_t const dsize =
        ZSTD_decompress(rt.data(), rt.size(), dst.data(), csize);
    ASSERT_FALSE(ZSTD_isError(dsize)) << ZSTD_getErrorName(dsize);
    ASSERT_EQ(dsize, src.size());
    rt.resize(dsize);
    EXPECT_EQ(rt, src);
  }
}

INSTANTIATE_TEST_SUITE_P(
    AllFiles, GoldenCompression,
    testing::ValuesIn(ListDir("golden-compression")),
    [](const testing::TestParamInfo<std::string>& info) {
      return ParamName(info.param);
    });

// Dictionary round trip: the http sample compressed with the upstream golden
// dictionary must decompress back byte-identically with the same dictionary.
TEST(GoldenDictionary, HttpRoundTrip) {
  const std::vector<unsigned char> dict =
      ReadFile(fs::path(DataDir()) / "golden-dictionaries" /
               "http-dict-missing-symbols");
  const std::vector<unsigned char> src =
      ReadFile(fs::path(DataDir()) / "golden-compression" / "http");

  ZSTD_CCtx* const cctx = ZSTD_createCCtx();
  ZSTD_DCtx* const dctx = ZSTD_createDCtx();
  ASSERT_NE(cctx, nullptr);
  ASSERT_NE(dctx, nullptr);

  std::vector<unsigned char> dst(ZSTD_compressBound(src.size()));
  std::size_t const csize = ZSTD_compress_usingDict(
      cctx, dst.data(), dst.size(), src.data(), src.size(), dict.data(),
      dict.size(), 3);
  ASSERT_FALSE(ZSTD_isError(csize)) << ZSTD_getErrorName(csize);

  std::vector<unsigned char> rt(src.size() + 1);
  std::size_t const dsize =
      ZSTD_decompress_usingDict(dctx, rt.data(), rt.size(), dst.data(), csize,
                                dict.data(), dict.size());
  ASSERT_FALSE(ZSTD_isError(dsize)) << ZSTD_getErrorName(dsize);
  ASSERT_EQ(dsize, src.size());
  rt.resize(dsize);
  EXPECT_EQ(rt, src);

  ZSTD_freeCCtx(cctx);
  ZSTD_freeDCtx(dctx);
}

// The zero-weight-dict sample must load as a dictionary and round-trip.
TEST(GoldenDictionary, ZeroWeightDictLoadsAndRoundTrips) {
  const std::vector<unsigned char> dict =
      ReadFile(fs::path(DataDir()) / "dict-files" / "zero-weight-dict");
  ZSTD_CDict* const cdict = ZSTD_createCDict(dict.data(), dict.size(), 3);
  ASSERT_NE(cdict, nullptr) << "zero-weight dictionary failed to load";
  ZSTD_DDict* const ddict = ZSTD_createDDict(dict.data(), dict.size());
  ASSERT_NE(ddict, nullptr);

  const std::string sample = "GET /index.html HTTP/1.1\r\nHost: example\r\n";
  ZSTD_CCtx* const cctx = ZSTD_createCCtx();
  ZSTD_DCtx* const dctx = ZSTD_createDCtx();
  std::vector<unsigned char> dst(ZSTD_compressBound(sample.size()));
  std::size_t const csize = ZSTD_compress_usingCDict(
      cctx, dst.data(), dst.size(), sample.data(), sample.size(), cdict);
  ASSERT_FALSE(ZSTD_isError(csize)) << ZSTD_getErrorName(csize);
  std::vector<unsigned char> rt(sample.size() + 1);
  std::size_t const dsize = ZSTD_decompress_usingDDict(
      dctx, rt.data(), rt.size(), dst.data(), csize, ddict);
  ASSERT_FALSE(ZSTD_isError(dsize)) << ZSTD_getErrorName(dsize);
  ASSERT_EQ(dsize, sample.size());
  rt.resize(dsize);
  EXPECT_EQ(std::string(rt.begin(), rt.end()), sample);

  ZSTD_freeCCtx(cctx);
  ZSTD_freeDCtx(dctx);
  ZSTD_freeCDict(cdict);
  ZSTD_freeDDict(ddict);
}

}  // namespace
