#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace stellar7 {
inline constexpr unsigned defaultNativeGameRate=25;
inline constexpr std::array<unsigned,5> nativeGameRates{25,35,50,70,0};
inline bool validNativeGameRate(const unsigned rate) {
    return std::find(nativeGameRates.begin(),nativeGameRates.end(),rate)!=nativeGameRates.end();
}

// Explicit portable speed policy, independent of recovered foreground CPU costs.
class NativeFramePacer {
public:
    explicit NativeFramePacer(const unsigned rate=0) {setRate(rate);}
    void setRate(const unsigned rate) {
        if(!validNativeGameRate(rate)) {throw std::invalid_argument("Unsupported native game rate.");}
        rate_=rate;reset();
    }
    unsigned rate() const noexcept {return rate_;}
    void reset() noexcept {next_.reset();remainder_=0;}
    std::uint64_t deadline(const std::uint64_t firstSample) {
        if(rate_==0) {return firstSample;}
        if(!next_||firstSample>*next_) {next_=firstSample;remainder_=0;}
        *next_+=sampleRate/rate_;remainder_+=sampleRate%rate_;
        if(remainder_>=rate_) {++*next_;remainder_-=rate_;}
        return *next_;
    }
private:
    static constexpr std::uint64_t sampleRate=44100;
    unsigned rate_=0,remainder_=0;
    std::optional<std::uint64_t> next_;
};
} // namespace stellar7
