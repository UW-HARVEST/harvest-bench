// Banner parsing and the C-facing accessors for the zstd scenario tables.
//
// Kept out of glue_fuzzer.c / glue_zstream.c because those translation units
// must be C (they include upstream sources verbatim, which are not valid C++).
// Everything here is ordinary C++ exposed through a C interface; nothing may
// throw across it. The tables themselves live in scenario_list.h.

#include <charconv>
#include <cstddef>
#include <string_view>

#include "scenario_list.h"
#include "scenario_record.h"

namespace {

// Upstream banners are "test%3i : ", "test%3d: " or "test%3u : "; a three-digit
// number fills the width and drops the leading space. Returns the scenario id,
// or -1 if the text is not a scenario banner.
int ParseScenarioId(std::string_view s) {
  constexpr std::string_view kPrefix = "test";
  if (s.substr(0, kPrefix.size()) != kPrefix) return -1;
  s.remove_prefix(kPrefix.size());
  while (!s.empty() && s.front() == ' ') s.remove_prefix(1);

  int value = 0;
  const auto* const first = s.data();
  const auto result = std::from_chars(first, first + s.size(), value);
  if (result.ec != std::errc{} || result.ptr == first) return -1;

  s.remove_prefix(static_cast<std::size_t>(result.ptr - first));
  while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
  return (!s.empty() && s.front() == ':') ? value : -1;
}

void OnBanner(std::string_view text) {
  const int id = ParseScenarioId(text);
  if (id >= 0) sr_scenario_start(id);
}

template <std::size_t N>
int IdAt(const std::array<Scenario, N>& table, int i) {
  if (i < 0 || static_cast<std::size_t>(i) >= table.size()) return -1;
  return table[static_cast<std::size_t>(i)].id;
}

template <std::size_t N>
const char* DescAt(const std::array<Scenario, N>& table, int i) {
  if (i < 0 || static_cast<std::size_t>(i) >= table.size()) return "";
  return table[static_cast<std::size_t>(i)].desc.data();
}

}  // namespace

// Called by the print hooks in the C glue for every line upstream emits.
extern "C" void harvest_fuzzer_on_banner(const char* text) noexcept {
  if (text != nullptr) OnBanner(text);
}

extern "C" void harvest_zstream_on_banner(const char* text) noexcept {
  if (text != nullptr) OnBanner(text);
}

extern "C" int harvest_fuzzer_scenario_count(void) noexcept {
  return static_cast<int>(kFuzzerScenarios.size());
}

extern "C" int harvest_fuzzer_scenario_id(int i) noexcept {
  return IdAt(kFuzzerScenarios, i);
}

extern "C" const char* harvest_fuzzer_scenario_desc(int i) noexcept {
  return DescAt(kFuzzerScenarios, i);
}

extern "C" int harvest_zstream_scenario_count(void) noexcept {
  return static_cast<int>(kZstreamScenarios.size());
}

extern "C" int harvest_zstream_scenario_id(int i) noexcept {
  return IdAt(kZstreamScenarios, i);
}

extern "C" const char* harvest_zstream_scenario_desc(int i) noexcept {
  return DescAt(kZstreamScenarios, i);
}
