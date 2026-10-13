#pragma once
#include <array>
#include <cstdint>
#include <istream>
#include <ostream>
#include <optional>

namespace jfg::boot {
// Internal replay proof, not a runtime clock policy. Reuse the recorded Count
// observations at identical guest instruction/idle boundaries so host speed
// cannot hide a semantic difference between native execution implementations.
class CountSampleTape {
public:
  bool record(std::ostream& stream) {
    if (input_ || output_ || finished_) return false;
    output_=&stream;
    const std::array<std::uint64_t,2> header{kMagic,46875000U};
    output_->write(reinterpret_cast<const char*>(header.data()),sizeof(header));
    failed_=!static_cast<bool>(*output_);return !failed_;
  }
  bool replay(std::istream& stream) {
    if (input_ || output_ || finished_) return false;
    input_=&stream;
    std::array<std::uint64_t,2> header{};
    input_->read(reinterpret_cast<char*>(header.data()),sizeof(header));
    failed_=!(*input_ && header==std::array<std::uint64_t,2>{kMagic,46875000U});return !failed_;
  }
  bool active() const { return input_ || output_; }
  bool replaying() const { return input_!=nullptr; }
  std::uint64_t samples() const { return samples_; }
  std::optional<std::uint64_t> sample(std::uint64_t instruction,std::uint32_t pc,
      std::uint32_t word,std::uint64_t idle_deadline,std::optional<std::uint64_t> actual) {
    if (!active()) return actual;
    if (finished_ || failed_ || samples_>=16000000U) return std::nullopt;
    std::array<std::uint64_t,4> row{instruction,(std::uint64_t(pc)<<32U)|word,idle_deadline,actual.value_or(0)};
    if (input_) {
      std::array<std::uint64_t,4> saved{};
      input_->read(reinterpret_cast<char*>(saved.data()),sizeof(saved));
      if (!*input_ || saved[0]!=row[0] || saved[1]!=row[1] || saved[2]!=row[2]) {
        failed_=true;return std::nullopt;
      }
      row=saved;
    } else if (!actual) { failed_=true;return std::nullopt; }
    if (row[0]<last_instruction_ || row[3]<last_count_ || row[3]<idle_deadline) {
      failed_=true;return std::nullopt;
    }
    if (output_) {
      output_->write(reinterpret_cast<const char*>(row.data()),sizeof(row));
      if (!*output_) { failed_=true;return std::nullopt; }
    }
    ++samples_;last_instruction_=row[0];last_count_=row[3];return row[3];
  }
  bool finish() {
    if (!active()) return true;
    if (finished_ || failed_) return false;
    finished_=true;
    if (output_) { output_->flush();return static_cast<bool>(*output_); }
    return input_->peek()==std::char_traits<char>::eof() && !input_->bad();
  }
private:
  static constexpr std::uint64_t kMagic=UINT64_C(0x31454c504d415343); // CSAMPLE1, LE fixture
  std::istream* input_=nullptr;
  std::ostream* output_=nullptr;
  std::uint64_t samples_=0,last_count_=0,last_instruction_=0;
  bool failed_=false,finished_=false;
};
}
