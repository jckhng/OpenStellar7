#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include "stellar7/controlled_frontend.hpp"
#include "stellar7/presentation_transport.hpp"
#include "stellar7/audio.hpp"
#include <array>
#include <chrono>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace stellar7 {
namespace {
constexpr unsigned maximumFrames = 64;
constexpr std::size_t audioSlots = 8;
constexpr unsigned displayWidth = QuietScanout::width * 2;
constexpr auto stallLimit = std::chrono::seconds(30);
constexpr DWORD hostPollMilliseconds = 1;
constexpr wchar_t windowClassName[] = L"Stellar7ControlledTransport320";

void audioCheck(MMRESULT result, const char* operation) {
    if (result != MMSYSERR_NOERROR) {
        throw std::runtime_error(std::string(operation) + " failed: " + std::to_string(result));
    }
}

class DevicePcm final : public PcmSink {
public:
    DevicePcm() {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 1;
        format.nSamplesPerSec = SoundBlasterSynth::sampleRate;
        format.wBitsPerSample = 16;
        format.nBlockAlign = sizeof(std::int16_t);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        audioCheck(waveOutOpen(&device_, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL), "waveOutOpen");
    }
    ~DevicePcm() {
        if (!device_) {
            return;
        }
        // If device cleanup fails, retain every address still owned by WinMM.
        // Normal/cancel paths use checked close; exceptional teardown cannot throw.
        if (!cleanup()) {
            storage_.release();
        }
    }
    DevicePcm(const DevicePcm&) = delete;
    DevicePcm& operator=(const DevicePcm&) = delete;

    TransportResult trySubmit(const std::vector<std::int16_t>& samples) override {
        if (!device_) {
            throw std::logic_error("controlled PCM device is closed");
        }
        if (samples.empty()) {
            return TransportResult::accepted;
        }
        if (samples.size() > std::numeric_limits<DWORD>::max() / sizeof(std::int16_t)
            || samples.size() >= (std::uint64_t{1} << 32) - submitted_) {
            throw std::invalid_argument("controlled PCM exceeds bounded TIME_SAMPLES domain");
        }
        for (auto& slot : storage_->slots) {
            if (slot.prepared && !(slot.header.dwFlags & WHDR_DONE)) {
                continue;
            }
            if (slot.prepared) {
                audioCheck(waveOutUnprepareHeader(device_, &slot.header, sizeof(slot.header)), "waveOutUnprepareHeader");
                slot.prepared = false;
            }
            slot.samples = samples;
            slot.header = {};
            slot.header.lpData = reinterpret_cast<LPSTR>(slot.samples.data());
            slot.header.dwBufferLength = static_cast<DWORD>(slot.samples.size() * sizeof(std::int16_t));
            audioCheck(waveOutPrepareHeader(device_, &slot.header, sizeof(slot.header)), "waveOutPrepareHeader");
            slot.prepared = true;
            audioCheck(waveOutWrite(device_, &slot.header, sizeof(slot.header)), "waveOutWrite");
            submitted_ += samples.size();
            return TransportResult::accepted;
        }
        return TransportResult::busy;
    }
    std::uint64_t playedSamples() override {
        MMTIME position{};
        position.wType = TIME_SAMPLES;
        audioCheck(waveOutGetPosition(device_, &position, sizeof(position)), "waveOutGetPosition");
        if (position.wType != TIME_SAMPLES || position.u.sample < played_ || position.u.sample > submitted_) {
            throw std::runtime_error("controlled PCM has invalid TIME_SAMPLES position");
        }
        played_ = position.u.sample;
        return played_;
    }
    bool complete() const {
        for (const auto& slot : storage_->slots) {
            if (slot.prepared && !(slot.header.dwFlags & WHDR_DONE)) {
                return false;
            }
        }
        return true;
    }
    std::uint64_t submitted() const { return submitted_; }
    void close() {
        audioCheck(waveOutReset(device_), "waveOutReset");
        for (auto& slot : storage_->slots) {
            if (slot.prepared) {
                audioCheck(waveOutUnprepareHeader(device_, &slot.header, sizeof(slot.header)), "waveOutUnprepareHeader");
                slot.prepared = false;
            }
        }
        audioCheck(waveOutClose(device_), "waveOutClose");
        device_ = nullptr;
    }
private:
    struct Slot { WAVEHDR header{}; std::vector<std::int16_t> samples; bool prepared = false; };
    struct Storage { std::array<Slot, audioSlots> slots; };
    bool cleanup() noexcept {
        if (waveOutReset(device_) != MMSYSERR_NOERROR) {
            return false;
        }
        for (auto& slot : storage_->slots) {
            if (slot.prepared) {
                if (waveOutUnprepareHeader(device_, &slot.header, sizeof(slot.header)) != MMSYSERR_NOERROR) {
                    return false;
                }
                slot.prepared = false;
            }
        }
        if (waveOutClose(device_) != MMSYSERR_NOERROR) {
            return false;
        }
        device_ = nullptr;
        return true;
    }
    std::unique_ptr<Storage> storage_ = std::make_unique<Storage>();
    HWAVEOUT device_ = nullptr;
    std::uint64_t submitted_ = 0, played_ = 0;
};

class ScanWindow final : public ScanSink, public ControlledKeyboardSource {
public:
    explicit ScanWindow(ControlledFrontendVisibility visibility) {
        WNDCLASSW type{};
        type.lpfnWndProc = procedure;
        type.hInstance = GetModuleHandleW(nullptr);
        type.lpszClassName = windowClassName;
        if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            throw std::runtime_error("controlled window class registration failed");
        }
        RECT bounds{0, 0, static_cast<LONG>(displayWidth), static_cast<LONG>(QuietScanout::height)};
        if (!AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE)) {
            throw std::runtime_error("controlled window sizing failed");
        }
        window_ = CreateWindowW(windowClassName, L"Stellar 7 controlled transport", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top,
            nullptr, nullptr, type.hInstance, this);
        if (!window_) {
            throw std::runtime_error("controlled window creation failed");
        }
        ShowWindow(window_, visibility == ControlledFrontendVisibility::shown ? SW_SHOW : SW_HIDE);
    }
    ~ScanWindow() { if (window_) { DestroyWindow(window_); } }
    ScanWindow(const ScanWindow&) = delete;
    ScanWindow& operator=(const ScanWindow&) = delete;
    TransportResult tryPresent(const QuietScanout::CompletedScan& scan) override {
        if (scan.rgb.size() != QuietScanout::frameBytes) {
            throw std::invalid_argument("controlled window requires complete BGR0 scan");
        }
        pixels_ = scan.rgb;
        HDC dc = GetDC(window_);
        if (!dc) {
            throw std::runtime_error("controlled window GetDC failed");
        }
        const bool drawn = blit(dc);
        ReleaseDC(window_, dc);
        if (!drawn) {
            throw std::runtime_error("controlled window StretchDIBits failed");
        }
        ++presented_;
        return TransportResult::accepted;
    }
    bool poll() {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { cancelled_ = true; }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (paintFailed_) { throw std::runtime_error("controlled window repaint failed"); }
        return !cancelled_;
    }
    std::uint64_t presented() const { return presented_; }
    std::uint64_t blits() const { return blits_; }
    std::optional<KeyboardEdge> forwardAtRenderReturn(const ControlledKeyboardBoundary& boundary) override {
        ++keyboardReturns_;
        if (!poll()) { return std::nullopt; }
        if (unsupportedKey_) { throw std::invalid_argument("controlled input currently admits Up only"); }
        if (inputFailed_) { throw std::runtime_error("controlled host key queue failed"); }
        const auto next = keys_.forwardAtRenderReturn(boundary);
        if (!next) { return std::nullopt; }
        ++keyboardEdges_;
        const auto edge = *next;
        keyboardEvents_.push_back({boundary.loopTick, edge});
        return edge;
    }
    std::uint64_t keyboardReturns() const { return keyboardReturns_; }
    std::uint64_t keyboardEdges() const { return keyboardEdges_; }
    const std::vector<ControlledFrontendKeyEdge>& keyboardEvents() const { return keyboardEvents_; }
private:
    bool blit(HDC dc) {
        if (pixels_.empty()) { return true; }
        RECT client{};
        if (!GetClientRect(window_, &client)) { return false; }
        BITMAPINFO bitmap{};
        bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap.bmiHeader.biWidth = QuietScanout::width;
        bitmap.bmiHeader.biHeight = -static_cast<LONG>(QuietScanout::height);
        bitmap.bmiHeader.biPlanes = 1;
        bitmap.bmiHeader.biBitCount = 32;
        bitmap.bmiHeader.biCompression = BI_RGB;
        const auto result = StretchDIBits(dc, 0, 0, client.right, client.bottom, 0, 0,
            QuietScanout::width, QuietScanout::height, pixels_.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
        if (result == GDI_ERROR) { return false; }
        ++blits_;
        return true;
    }
    static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM key, LPARAM value) {
        auto* owner = reinterpret_cast<ScanWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            owner = static_cast<ScanWindow*>(reinterpret_cast<CREATESTRUCTW*>(value)->lpCreateParams);
            owner->window_ = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(owner));
        }
        if (owner) {
            if (message == WM_CLOSE || (message == WM_KEYDOWN && key == VK_ESCAPE)) {
                owner->cancelled_ = true;
                return 0;
            }
            if ((message == WM_KEYDOWN || message == WM_KEYUP
                || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP) && key == VK_UP) {
                constexpr int pressedHostKeyMask = 1 << 15;
                if (message == WM_SYSKEYDOWN || message == WM_SYSKEYUP
                    || (GetKeyState(VK_SHIFT) & pressedHostKeyMask) != 0
                    || (GetKeyState(VK_CONTROL) & pressedHostKeyMask) != 0
                    || (GetKeyState(VK_MENU) & pressedHostKeyMask) != 0) {
                    owner->unsupportedKey_ = true;
                    return 0;
                }
                try {
                    owner->keys_.transition(message == WM_KEYDOWN ? ControlledForwardState::held : ControlledForwardState::released);
                } catch (...) { owner->inputFailed_ = true; }
                return 0;
            }
            if (message == WM_KILLFOCUS) {
                try { owner->keys_.transition(ControlledForwardState::released); }
                catch (...) { owner->inputFailed_ = true; }
            }
            if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && (key == VK_LEFT || key == VK_RIGHT || key == VK_DOWN
                || key == VK_SPACE || key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU)) {
                owner->unsupportedKey_ = true;
            }
            if (message == WM_PAINT) {
                PAINTSTRUCT paint{};
                HDC dc = BeginPaint(window, &paint);
                if (!dc || !owner->blit(dc)) { owner->paintFailed_ = true; }
                EndPaint(window, &paint);
                return 0;
            }
        }
        return DefWindowProcW(window, message, key, value);
    }
    HWND window_ = nullptr;
    std::vector<std::uint8_t> pixels_;
    bool cancelled_ = false, paintFailed_ = false;
    bool unsupportedKey_ = false, inputFailed_ = false;
    ControlledForwardQueue keys_;
    std::uint64_t keyboardReturns_ = 0, keyboardEdges_ = 0;
    std::vector<ControlledFrontendKeyEdge> keyboardEvents_;
    std::uint64_t presented_ = 0, blits_ = 0;
};
void validateFrontend(unsigned completeFrames, ControlledFrontendVisibility visibility) {
    if (completeFrames == 0 || completeFrames > maximumFrames ||
        (visibility != ControlledFrontendVisibility::shown && visibility != ControlledFrontendVisibility::hidden)) {
        throw std::invalid_argument("controlled frontend requires 1..64 frames and known visibility");
    }
}

ControlledFrontendResult runTransport(ControlledLevel2Session& session, unsigned completeFrames,
    DevicePcm& audio, ScanWindow& window) {
    PresentationTransport transport(audio, window, audioSlots);
    ControlledFrontendResult result;
    auto progressAt = std::chrono::steady_clock::now();
    std::uint64_t lastPlayed = 0, lastScans = 0;
    const auto wait = [&]() {
        if (!window.poll()) {
            result.cancelled = true;
            return false;
        }
        const auto played = audio.playedSamples();
        if (played != lastPlayed || window.presented() != lastScans) {
            lastPlayed = played;
            lastScans = window.presented();
            progressAt = std::chrono::steady_clock::now();
        }
        if (std::chrono::steady_clock::now() - progressAt > stallLimit) {
            throw std::runtime_error("controlled transport stalled for 30 seconds: submitted="
                + std::to_string(audio.submitted()) + " played=" + std::to_string(lastPlayed)
                + " scans=" + std::to_string(lastScans));
        }
        if (MsgWaitForMultipleObjects(0, nullptr, FALSE, hostPollMilliseconds, QS_ALLINPUT) == WAIT_FAILED) {
            throw std::runtime_error("controlled transport host wait failed");
        }
        return true;
    };
    const auto deliver = [&](ControlledSessionDelivery delivery) {
        while (transport.tryAccept(delivery) == TransportResult::busy) {
            transport.pump();
            if (!wait()) { return false; }
        }
        transport.pump();
        return true;
    };
    if (deliver(session.takeDelivery())) {
        for (unsigned frame = 0; frame < completeFrames; ++frame) {
            if (!window.poll()) { result.cancelled = true; break; }
            auto delivery = session.advance({});
            ++result.frames;
            if (!deliver(std::move(delivery))) { break; }
        }
    }
    if (!result.cancelled) {
        while (transport.hasPending()) {
            transport.pump();
            if (!wait()) { break; }
        }
    }
    if (!result.cancelled) {
        while (audio.playedSamples() < audio.submitted() || !audio.complete()) {
            if (!wait()) { break; }
        }
    }
    result.samplesSubmitted = audio.submitted();
    result.samplesPlayed = audio.playedSamples();
    result.scansPresented = window.presented();
    result.gdiBlits = window.blits();
    result.keyboardReturns = window.keyboardReturns();
    result.keyboardEdges = window.keyboardEdges();
    result.keyboardEvents = window.keyboardEvents();
    if (result.cancelled) { transport.cancel(); }
    audio.close();
    return result;
}
}

ControlledFrontendResult runControlledSession(ControlledLevel2Session& session,
    unsigned completeFrames, ControlledFrontendVisibility visibility) {
    validateFrontend(completeFrames, visibility);
    DevicePcm audio;
    ScanWindow window(visibility);
    return runTransport(session, completeFrames, audio, window);
}

ControlledFrontendResult runControlledInput(GameplayAssets assets, ControlledBootstrapRequest request,
    unsigned completeFrames, ControlledFrontendVisibility visibility) {
    validateFrontend(completeFrames, visibility);
    request.arrivalInput = ControlledArrivalInput::hostForward;
    DevicePcm audio;
    ScanWindow window(visibility);
    ControlledLevel2Session session(std::move(assets), std::move(request), nullptr, &window);
    return runTransport(session, completeFrames, audio, window);
}
}
