#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include "stellar7/frontend.hpp"

#include "stellar7/audio.hpp"
#include "stellar7/gameplay.hpp"
#include "stellar7/input_movie.hpp"
#include "stellar7/native_session.hpp"
#include "stellar7/recovered_frontend.hpp"
#include "win32_joystick.hpp"
#include "win32_source_keys.hpp"
#include "win32_host_keys.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <functional>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace stellar7 {
namespace {

constexpr int sourceWidth = 320;
constexpr int sourceHeight = 200;
constexpr int scale = 3;
constexpr wchar_t windowClassName[] = L"Stellar7PortableGameplay";

template<class Present>
void presentRecoveredFrames(std::deque<NativeTimedFrame>& frames,const std::uint64_t played,Present present) {
    while(!frames.empty()&&frames.front().sampleEnd<=played) {
        present(frames.front().indexed);frames.pop_front();
    }
}

void queueRecoveredContinueScan(const UINT message, const LPARAM keyData,
    std::deque<std::uint8_t>& scans) {
    if (message != WM_KEYDOWN && message != WM_SYSKEYDOWN) { return; }
    constexpr unsigned scanShift = 16;
    constexpr unsigned scanMask = 0xff;
    constexpr std::uint8_t escapeScan = 0x01;
    const auto scan = static_cast<std::uint8_t>((static_cast<std::uintptr_t>(keyData) >> scanShift) & scanMask);
    if (scan != 0 && scan != escapeScan) { scans.push_back(scan); } //Escape is host exit.
}

ContinueKeyboardPacket recoveredContinuePacket(const std::deque<std::uint8_t>& scans) {
    ContinueKeyboardPacket packet;
    packet.peekScan = scans.empty() ? 0 : scans.front();
    const auto held = [](const int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; };
    packet.enter = held(VK_RETURN);
    packet.space = held(VK_SPACE);
    constexpr std::array<int,8> sourceKeys{VK_END,VK_DOWN,VK_NEXT,VK_HOME,VK_UP,VK_PRIOR,VK_LEFT,VK_RIGHT};
    for (std::size_t index = 0; index < sourceKeys.size(); ++index) { packet.directions[index] = held(sourceKeys[index]); }
    return packet;
}

class Win32Audio {
public:
    enum class Transport { immediate, continuous };

    explicit Win32Audio(const Transport transport = Transport::immediate) : transport_(transport) {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 1;
        format.nSamplesPerSec = SoundBlasterSynth::sampleRate;
        format.wBitsPerSample = 16;
        format.nBlockAlign = 2;
        format.nAvgBytesPerSec =
            format.nSamplesPerSec * format.nBlockAlign;
        if (
            waveOutOpen(
                &device_,
                WAVE_MAPPER,
                &format,
                0,
                0,
                CALLBACK_NULL
            ) != MMSYSERR_NOERROR
        ) {
            throw std::runtime_error("waveOutOpen failed");
        }
    }

    ~Win32Audio() {
        if (device_ == nullptr) {
            return;
        }
        waveOutReset(device_);
        for (auto& buffer : buffers_) {
            if (buffer.prepared) {
                waveOutUnprepareHeader(
                    device_,
                    &buffer.header,
                    sizeof(buffer.header)
                );
            }
        }
        waveOutClose(device_);
    }

    bool submit(const std::vector<std::int16_t>& samples) {
        if (transport_ == Transport::immediate) {
            if (!samples.empty() && !submitSamples(samples)) { return false; }
            queuedSamples_ += samples.size();
            return true;
        }
        for (std::size_t offset = 0; offset < samples.size();) {
            const auto count = std::min(packetSamples-pendingSamples_.size(),samples.size()-offset);
            pendingSamples_.insert(pendingSamples_.end(),samples.begin()+offset,samples.begin()+offset+count);
            queuedSamples_ += count;
            offset += count;
            if (pendingSamples_.size() == packetSamples) {
                if (!submitSamples(pendingSamples_)) { return false; }
                pendingSamples_.clear();
            }
        }
        return true;
    }

    bool drain() {
        if (!pendingSamples_.empty()) {
            if (!submitSamples(pendingSamples_)) { return false; }
            pendingSamples_.clear();
        }
        for (auto& buffer : buffers_) {
            if (!release(buffer)) {
                return false;
            }
        }
        return true;
    }

    void setHostService(std::function<bool()> service) { hostService_ = std::move(service); }
    std::uint64_t playbackSamples() { return playedSamples(); }

    std::uint64_t queuedSamples() const noexcept { return queuedSamples_; }
    std::uint64_t submittedSamples() const noexcept { return submittedSamples_; }
    std::uint64_t completedSamples() const noexcept { return completedSamples_; }

    bool waitForInput() {
        if(transport_!=Transport::continuous) {return true;}
        while(queuedSamples_>playedSamples()+interactiveLeadSamples) {
            if(hostService_&&!hostService_()) {return false;}
            Sleep(1);
        }
        return true;
    }

private:
    struct Buffer {
        std::vector<std::int16_t> samples;
        WAVEHDR header{};
        bool prepared = false;
    };

    bool release(Buffer& buffer) {
        if (!buffer.prepared) {
            return true;
        }
        while ((buffer.header.dwFlags & WHDR_DONE) == 0) {
            if (hostService_) {
                if (!hostService_()) { return false; }
                Sleep(1);
                continue;
            }
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    return false;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            Sleep(1);
        }
        if (waveOutUnprepareHeader(device_, &buffer.header, sizeof(buffer.header)) != MMSYSERR_NOERROR) {
            throw std::runtime_error("waveOutUnprepareHeader failed");
        }
        buffer.prepared = false;
        completedSamples_ += buffer.samples.size();
        return true;
    }

    std::uint64_t playedSamples() {
        MMTIME position{};
        position.wType = TIME_SAMPLES;
        if (waveOutGetPosition(device_, &position, sizeof(position)) != MMSYSERR_NOERROR
            || position.wType != TIME_SAMPLES) {
            throw std::runtime_error("audio device does not expose its sample playback cursor");
        }
        if (position.u.sample < lastPlaybackPosition_) { playbackWrap_ += std::uint64_t{1} << 32; }
        lastPlaybackPosition_ = position.u.sample;
        return playbackWrap_ + lastPlaybackPosition_;
    }

    bool submitSamples(std::vector<std::int16_t> samples) {
        Buffer& buffer = buffers_[nextBuffer_];
        if (!release(buffer)) {
            return false;
        }
        buffer.samples = std::move(samples);
        buffer.header = {};
        buffer.header.lpData = reinterpret_cast<char*>(
            buffer.samples.data()
        );
        buffer.header.dwBufferLength = static_cast<DWORD>(
            buffer.samples.size() * sizeof(std::int16_t)
        );
        if (waveOutPrepareHeader(device_, &buffer.header, sizeof(buffer.header)) != MMSYSERR_NOERROR) {
            throw std::runtime_error("waveOutPrepareHeader failed");
        }
        buffer.prepared = true;
        if (waveOutWrite(device_, &buffer.header, sizeof(buffer.header)) != MMSYSERR_NOERROR) {
            throw std::runtime_error("waveOutWrite failed");
        }
        submittedSamples_ += buffer.samples.size();
        const auto activeBuffers=transport_==Transport::continuous?continuousBuffers:buffers_.size();
        nextBuffer_ = (nextBuffer_ + 1) % activeBuffers;
        return true;
    }

    std::function<bool()> hostService_;
    static constexpr std::size_t packetSamples = 512;
    static constexpr std::size_t continuousBuffers = 8;
    static constexpr std::uint64_t interactiveLeadSamples = 5*packetSamples;
    Transport transport_;
    std::vector<std::int16_t> pendingSamples_;
    HWAVEOUT device_ = nullptr;
    std::array<Buffer, 8> buffers_{};
    std::size_t nextBuffer_ = 0;
    DWORD lastPlaybackPosition_ = 0;
    std::uint64_t playbackWrap_ = 0;
    std::uint64_t queuedSamples_ = 0;
    std::uint64_t submittedSamples_ = 0;
    std::uint64_t completedSamples_ = 0;
};

class Win32Gameplay {
public:
    Win32Gameplay(
        const GameplayAssets& assets,
        GameplayModel initialModel,
        const GameplayRunOptions& options,
        const std::vector<GameplayInput>* smokeInputs = nullptr,
        const NativeStartupAssets* startupAssets = nullptr,
        FrameCopyProfile recoveredCopyProfile = FrameCopyProfile::unknown,
        CampaignExecution execution = CampaignExecution::sourceWork
    )
        : options_(options),
          smokeInputs_(smokeInputs),
          assets_(assets), startupAssets_(startupAssets),
          session_(startupAssets ? nullptr : std::make_unique<NativeGameplaySession>(assets, std::move(initialModel),
              options.startAtLevel3 ? NativeSessionStart::level3
                  : options.startAtLevel2 ? NativeSessionStart::level2 : NativeSessionStart::level1,
              std::unique_ptr<NativeAudioOwner>{}, options.highscoreStore)),
          recovered_(recoveredCopyProfile == FrameCopyProfile::unknown ? nullptr
              : std::make_unique<RecoveredFrontend>(assets, *startupAssets, recoveredCopyProfile,
                  10000, options.highscoreStore, execution,execution==CampaignExecution::playable && GetSystemMetrics(SM_MOUSEPRESENT)!=0
                      ? NativePointerCapability::portablePointer : NativePointerCapability::unavailable,options.menuFeatures,initialJoystick(execution))),
          audio_(recovered_ ? Win32Audio::Transport::continuous : Win32Audio::Transport::immediate),
          bgra_(sourceWidth * sourceHeight) {
        bitmapInfo_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo_.bmiHeader.biWidth = sourceWidth;
        bitmapInfo_.bmiHeader.biHeight = -sourceHeight;
        bitmapInfo_.bmiHeader.biPlanes = 1;
        bitmapInfo_.bmiHeader.biBitCount = 32;
        bitmapInfo_.bmiHeader.biCompression = BI_RGB;
    }

    ~Win32Gameplay() {
        if (instance_ == this) {
            instance_ = nullptr;
        }
        if (window_ != nullptr) {
            DestroyWindow(window_);
        }
    }

    int run() {
        instance_ = this;
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = windowProcedure;
        windowClass.hInstance = instance;
        windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        windowClass.lpszClassName = windowClassName;
        if (RegisterClassW(&windowClass) == 0
            && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            throw std::runtime_error("RegisterClassW failed");
        }

        RECT rectangle{0, 0, sourceWidth * scale, sourceHeight * scale};
        AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE);
        window_ = CreateWindowExW(
            0,
            windowClassName,
            L"Stellar 7 portable gameplay - arrows steer/throttle, Space fires",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rectangle.right - rectangle.left,
            rectangle.bottom - rectangle.top,
            nullptr,
            nullptr,
            instance,
            nullptr
        );
        if (window_ == nullptr) {
            throw std::runtime_error("CreateWindowExW failed");
        }
        ShowWindow(window_, smokeInputs_ ? SW_SHOWNOACTIVATE : SW_SHOW);

        if (recovered_) { return runRecovered(); }

        if (startupAssets_ && !presentStartup()) {
            return 0;
        }
        bool running = presentDelivery(session_->initialize());
        bool completed = false;
        auto nextTick = std::chrono::steady_clock::now();
        while (running) {
            if (smokeInputs_ && smokeResult_.frames == smokeInputs_->size()) {
                completed = true;
                break;
            }
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    running = false;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (!running) {
                break;
            }
            const bool exitRequested =
                (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

            const bool continuePrompt = session_->model().phase() == GameplayPhase::continuePrompt;
            const bool nameEntry = session_->model().phase() == GameplayPhase::highscoreNameEntry;
            const bool hall = session_->model().phase() == GameplayPhase::highscoreHall;
            if (exitRequested && !continuePrompt && !nameEntry && !hall) {
                break;
            }
            const GameplayInput input = smokeInputs_ ? (*smokeInputs_)[smokeResult_.frames] : GameplayInput{
                (GetAsyncKeyState(VK_LEFT) & 0x8000) != 0,
                (GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0,
                (GetAsyncKeyState(VK_UP) & 0x8000) != 0,
                (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0,
                (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0,
                (continuePrompt || hall) && exitRequested,
                (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0,
            };
            const NameEntryInput text{std::exchange(nameCharacters_, {})};
            if (!text.characters.empty() && !options_.inputRecordingPath.empty()) {
                throw std::invalid_argument("native text input recording is not yet supported");
            }
            if (!options_.inputRecordingPath.empty()) {
                inputRecorder_.observe(
                    static_cast<std::uint32_t>(session_->model().ticks() + 1),
                    input
                );
            }
            const auto delivery = session_->model().phase() == GameplayPhase::nativeMenu
                ? session_->advanceMenu({input.forward, input.reverse, input.confirm}, 512)
                : session_->advance(input, text);
            if (!presentDelivery(delivery)) {
                break;
            }
            smokeResult_.audioEvents += delivery.events.size();
            if (session_->model().phase() == GameplayPhase::campaignEnded) {
                completed = true;
                break;
            }
            if (!delivery.entry.empty()) {
                nextTick = std::chrono::steady_clock::now();
            }
            std::wstring title =
                L"Stellar 7 portable gameplay | tick "
                + std::to_wstring(session_->model().ticks())
                + L" | armor "
                + std::to_wstring(session_->model().playerArmor())
                + L" | score "
                + std::to_wstring(session_->model().score())
                + L" | range "
                + std::to_wstring(session_->model().enemyRange())
                + L" | wave "
                + std::to_wstring(
                    session_->model().level1State().activeEnemyCount()
                )
                + L" active, "
                + std::to_wstring(
                    session_->model().level1State().spawnedEnemyCount()
                )
                + L"/"
                + std::to_wstring(
                    session_->model().level1State().maximumEnemyCount()
                )
                + L" spawned | guardian "
                + (
                    session_->model().level1State().postWaveObject() != nullptr
                        ? L"active"
                        : L"not spawned"
                );
            if (session_->model().phase() == GameplayPhase::level2Handoff) {
                title += L" | Level 2 handoff";
            } else if (session_->model().phase() == GameplayPhase::level2) {
                title += L" | Level 2 tick "
                    + std::to_wstring(session_->model().level2LoopTick());
            } else if (
                session_->model().phase() == GameplayPhase::level2SuccessPresentation
            ) {
                title += L" | Level 2 complete";
            } else if (session_->model().phase() == GameplayPhase::level3Handoff) {
                title += L" | Level 3 handoff";
            } else if (session_->model().phase() == GameplayPhase::level3) {
                title += L" | Level 3 tick "
                    + std::to_wstring(session_->model().level2LoopTick());
            } else if (
                session_->model().phase() == GameplayPhase::level3SuccessPresentation
            ) {
                title += L" | Level 3 complete";
            } else if (session_->model().phase() == GameplayPhase::level4Handoff) {
                title += L" | Level 4 handoff";
            } else if (session_->model().phase() == GameplayPhase::continuePrompt) {
                title += L" | Continue? Up/Down select, Enter confirms, Escape declines";
            } else if (session_->model().phase() == GameplayPhase::nativeMenu) {
                title = L"Stellar 7 | Main menu";
            }
            SetWindowTextW(window_, title.c_str());
            presentFrame(session_->frame());
            ++smokeResult_.frames;
            if (session_->pageObservation().projection == NativePageProjection::retainedNativeLevel1) {
                ++smokeResult_.retainedLevel1Frames;
            }
            if (smokeInputs_ && (smokeResult_.phases.empty()
                || smokeResult_.phases.back().phase != session_->model().phase())) {
                smokeResult_.phases.push_back({smokeResult_.frames, session_->model().phase(),
                    session_->elapsedSamples(), session_->synth().registerWrites().size(), smokeResult_.gdiBlits,
                    session_->frame()});
            }

            if (session_->pageObservation().projection == NativePageProjection::retainedLevel2Success
                || session_->pageObservation().projection == NativePageProjection::retainedMenu) {
                nextTick += std::chrono::nanoseconds(delivery.samples.size() * 1'000'000'000ULL
                    / SoundBlasterSynth::sampleRate);
            } else {
                nextTick += std::chrono::milliseconds(33);
            }
            std::this_thread::sleep_until(nextTick);
            if (std::chrono::steady_clock::now() - nextTick
                > std::chrono::milliseconds(250)) {
                nextTick = std::chrono::steady_clock::now();
            }
        }
        if (completed) {
            completed = audio_.drain();
        }
        smokeResult_.cancelled = !completed;
        smokeResult_.generatedSamples = session_->elapsedSamples();
        smokeResult_.queuedSamples = audio_.queuedSamples();
        smokeResult_.completedSamples = audio_.completedSamples();
        smokeResult_.oplWrites = session_->synth().registerWrites().size();
        smokeResult_.finalFrame = session_->frame();
        if (completed && (smokeResult_.generatedSamples != smokeResult_.queuedSamples
            || smokeResult_.queuedSamples != smokeResult_.completedSamples)) {
            throw std::logic_error("native frontend lost generated PCM");
        }
        DestroyWindow(window_);
        window_ = nullptr;
        instance_ = nullptr;
        if (!options_.inputRecordingPath.empty()) {
            writeGameplayInputMovie(
                options_.inputRecordingPath,
                inputRecorder_.finish(
                    static_cast<std::uint32_t>(session_->model().ticks())
                )
            );
        }
        return 0;
    }

    GameplaySmokeResult smokeResult() const { return smokeResult_; }

    void paint() {
        PAINTSTRUCT paint{};
        const HDC device = BeginPaint(window_, &paint);
        RECT client{};
        GetClientRect(window_, &client);
        SetStretchBltMode(device, COLORONCOLOR);
        const int rows = StretchDIBits(
            device,
            0,
            0,
            client.right,
            client.bottom,
            0,
            0,
            sourceWidth,
            sourceHeight,
            bgra_.data(),
            &bitmapInfo_,
            DIB_RGB_COLORS,
            SRCCOPY
        );
        EndPaint(window_, &paint);
        if (rows == 0 || rows == GDI_ERROR) {
            paintFailed_ = true;
            return;
        }
        ++smokeResult_.gdiBlits;
        paintedRevision_ = pixelsRevision_;
    }

private:
    bool recoveredHeld(const unsigned key) const {
        return recovered_->execution()==CampaignExecution::playable?recoveredKeys_.held(key)
            :(GetAsyncKeyState(static_cast<int>(key))&0x8000)!=0;
    }

    NativeJoystickSample initialJoystick(const CampaignExecution execution) {
        if(execution==CampaignExecution::playable&&!options_.inputRecordingPath.empty()
            &&hasNativeMenuFeature(options_.menuFeatures,NativeMenuFeatures::automaticAttract)) {
            throw std::invalid_argument("input movies do not encode the automatic attract profile");
        }
        if(execution!=CampaignExecution::playable
            ||!hasNativeMenuFeature(options_.menuFeatures,NativeMenuFeatures::joystickControls)) {return {};}
        const auto sample=joystickHost_.poll();
        validateJoystickRecording(sample);return sample;
    }

    void validateJoystickRecording(const NativeJoystickSample sample) const {
        if(sample.connected&&!options_.inputRecordingPath.empty()) {
            throw std::invalid_argument("input movies do not encode joystick packets");
        }
    }

    std::optional<NativeJoystickSample> recoveredJoystick() {
        if(recovered_->execution()!=CampaignExecution::playable
            ||!hasNativeMenuFeature(options_.menuFeatures,NativeMenuFeatures::joystickControls)) {return std::nullopt;}
        if(GetForegroundWindow()!=window_) {
            joystickHost_.suspend();
            NativeJoystickSample sample;sample.phase=NativeJoystickPhase::cancel;return sample;
        }
        const auto sample=joystickHost_.poll();validateJoystickRecording(sample);return sample;
    }

    bool recoveredMainMenu() const noexcept {
        return recovered_->session() ? recovered_->session()->model().phase()==GameplayPhase::nativeMenu
            : recovered_->startup().phase()==NativeStartupPhase::awaitingMenuDecision;
    }

    bool recoveredAttract() const noexcept {return recovered_->attractProgress().has_value();}

    bool sourceKeysEnabled() const noexcept {
        return hasNativeMenuFeature(options_.menuFeatures,NativeMenuFeatures::automaticAttract);
    }

    void finishSourceInput(const std::uint16_t delivered,const bool menuPolled) {
        if(delivered!=0) {recoveredSourceKeys_.pop();}
        else if(menuPolled&&!recoveredAttract()) {recoveredSourceKeys_.pop();}
        if(!recoveredAttract()&&!recoveredMainMenu()&&!recovered_->presentationInputActive()) {recoveredSourceKeys_.clear();}
    }

    bool pumpRecoveredHost() {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { return false; }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if(recoveredRecordingRejected_) {throw std::invalid_argument("input movies do not encode calibration requests");}
        const bool pointerUnfocused=recovered_->execution()==CampaignExecution::playable
            &&GetForegroundWindow()!=window_;
        const bool escape = !pointerUnfocused&&(GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        if (!escape) { recoveredMenuEscapeConsumed_ = false; }
        return !recoveredExitRequested_ && (recovered_->execution()==CampaignExecution::playable || !escape || recoveredMenuEscapeConsumed_ || recovered_->menuBackAvailable() || recovered_->presentationInputActive()
            || recovered_->joystickCalibrationActive() || recoveredAttract() || (sourceKeysEnabled()&&recoveredMainMenu()));
    }

    bool serviceRecoveredHost() {
        if (!pumpRecoveredHost()) { return false; }
        const auto played = audio_.playbackSamples();
        presentRecoveredFrames(recoveredPresentations_,played,[this](const IndexedScreen& frame) {presentFrame(frame);});
        return true;
    }

    std::optional<NativePointerInput> recoveredPointer() const {
        if(recovered_->execution()!=CampaignExecution::playable || GetSystemMetrics(SM_MOUSEPRESENT)==0) {
            return std::nullopt;
        }
        const auto cancelled=[]() {
            NativePointerInput input;input.phase=NativePointerPhase::cancel;return input;
        };
        if(GetForegroundWindow()!=window_) {return cancelled();}
        POINT point{};RECT bounds{};
        if(!GetCursorPos(&point)||!ScreenToClient(window_,&point)||!GetClientRect(window_,&bounds)
            || bounds.right<=0||bounds.bottom<=0) {return cancelled();}
        NativePointerInput input;
        input.position={static_cast<std::int16_t>(std::clamp<int>(point.x*sourceWidth/bounds.right,0,319)),
            static_cast<std::int16_t>(std::clamp<int>(point.y*sourceHeight/bounds.bottom,0,196))};
        input.primary=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
        input.secondary=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;
        return input;
    }

    void positionRecoveredPointer(const ProjectedPoint target) const {
        if(GetForegroundWindow()!=window_) {return;}
        RECT bounds{};POINT point{};
        if(!GetClientRect(window_,&bounds)||bounds.right<=0||bounds.bottom<=0) {return;}
        point.x=target.x*bounds.right/sourceWidth;
        point.y=target.y*bounds.bottom/sourceHeight;
        if(ClientToScreen(window_,&point)) {SetCursorPos(point.x,point.y);}
    }

    bool queueRecovered(const NativeSessionDelivery& delivery) {
        if(delivery.pointerPosition) {
            const auto sample=recoveredPointer();
            if(sample && sample->phase==NativePointerPhase::sample && (sample->position.x!=delivery.pointerPosition->x
                || sample->position.y!=delivery.pointerPosition->y)) {positionRecoveredPointer(*delivery.pointerPosition);}
        }
        if (delivery.firstSample != audio_.queuedSamples() || !delivery.entry.empty()) {
            throw std::logic_error("unsupported recovered frontend delivery boundary");
        }
        const auto sampleEnd = delivery.firstSample + delivery.samples.size();
        for (const auto& frame : delivery.presentations) {
            if (frame.sampleEnd < delivery.firstSample || frame.sampleEnd > sampleEnd
                || (!recoveredPresentations_.empty() && frame.sampleEnd < recoveredPresentations_.back().sampleEnd)) {
                throw std::logic_error("recovered presentation is outside its source delivery");
            }
            recoveredPresentations_.push_back(frame);
        }
        constexpr std::size_t transportPacketSamples = 512;
        for (std::size_t offset = 0; offset < delivery.samples.size(); offset += transportPacketSamples) {
            if (!serviceRecoveredHost()) { return false; }
            const auto count = std::min(transportPacketSamples, delivery.samples.size() - offset);
            if (!audio_.submit(std::vector<std::int16_t>(delivery.samples.begin() + offset,
                delivery.samples.begin() + offset + count))) { return false; }
        }
        return serviceRecoveredHost();
    }

    bool deliverRecovered(const NativeSessionDelivery& delivery, const std::size_t streamedSamples) {
        if (streamedSamples > delivery.samples.size()
            || delivery.firstSample+streamedSamples != audio_.queuedSamples()) {
            throw std::logic_error("recovered frontend streamed prefix is not contiguous");
        }
        auto remaining = delivery;
        remaining.firstSample += streamedSamples;
        remaining.samples.erase(remaining.samples.begin(),remaining.samples.begin()+streamedSamples);
        if (!queueRecovered(remaining)) { return false; }
        if (audio_.queuedSamples() != recovered_->elapsedSamples()) {
            throw std::logic_error("recovered frontend lost generated PCM");
        }
        return serviceRecoveredHost();
    }

    int runRecovered() {
        audio_.setHostService([this]() { return serviceRecoveredHost(); });
        while (serviceRecoveredHost()) {
            if(!audio_.waitForInput()) {break;}
            recoveredKeys_.poll();
            if(!recoveredHeld(VK_ESCAPE)) {recoveredPauseEscapeHeld_=false;}
            NativeSessionDelivery delivery;
            std::size_t streamedSamples = 0;
            bool running = true;
            const bool focused=GetForegroundWindow()==window_;
            const auto joystick=recoveredJoystick();
            const bool menuPolled=recoveredMainMenu();
            const std::uint16_t sourceKey=focused&&recovered_->attractAcceptsSourceKey()?recoveredSourceKeys_.front():0;
            if(!focused) {
                recoveredSourceKeys_.clear();recoveredSystemActions_.clear();recoveredContinueScans_.clear();
                recoveredCalibrationRequested_=false;recoveredPresentationKey_=false;recoveredPresentationEscape_=false;recoveredKeys_.clear();
            }
            if (recovered_->session()) {
                const auto& model = recovered_->session()->model();
                const auto phase = model.phase();
                const bool playable = recovered_->execution() == CampaignExecution::playable;
                const bool presentation=playable&&recovered_->presentationInputActive();
                const bool livePreferences=playable&&recovered_->menuBackAvailable()
                    &&phase!=GameplayPhase::nativeMenu;
                const bool calibration=playable&&recovered_->joystickCalibrationActive();
                const bool attract=playable&&recoveredAttract();
                const bool modal = calibration || attract || presentation || livePreferences || phase == GameplayPhase::continuePrompt || phase == GameplayPhase::highscoreNameEntry
                    || phase == GameplayPhase::highscoreHall || phase == GameplayPhase::nativeMenu
                    || phase == GameplayPhase::nativeBeginWait;
                GameplayInput input{
                    recoveredHeld(VK_LEFT),
                    recoveredHeld(VK_RIGHT),
                    recoveredHeld(VK_UP),
                    recoveredHeld(VK_DOWN),
                    recoveredHeld(VK_SPACE),
                    playable && (recovered_->menuBackAvailable()||presentation) && !recoveredPauseEscapeHeld_ && recoveredHeld(VK_ESCAPE),
                    playable && modal && recoveredHeld(VK_RETURN)};
                if(presentation&&!attract) {
                    input.confirm=std::exchange(recoveredPresentationKey_,false)||input.confirm;
                    input.exit=std::exchange(recoveredPresentationEscape_,false)||input.exit;
                    if(input.exit) {recoveredMenuEscapeConsumed_=true;}
                    if(!recovered_->session()->storyScene()) {input={};} //The retry caller consumes no host input.
                } else {recoveredPresentationKey_=false;recoveredPresentationEscape_=false;}
                if(!focused&&playable) {input={};}
                if(calibration) {input.exit=false;}
                if(playable&&modal) {input.pointer=recoveredPointer();}
                input.joystick=joystick;input.sourceKey=sourceKey;
                const bool ordinary = phase == GameplayPhase::level1 || phase == GameplayPhase::level2
                    || phase == GameplayPhase::level3Native || phase == GameplayPhase::playable
                    || phase == GameplayPhase::level5;
                if(playable&&ordinary&&!livePreferences&&!presentation&&!calibration&&!attract
                    &&recoveredKeys_.pressed(VK_ESCAPE)
                    &&hasNativeMenuFeature(options_.menuFeatures,NativeMenuFeatures::fullGraphicDetail)) {
                    input.systemAction=GameplaySystemAction::preferences;recoveredPauseEscapeHeld_=true;
                }
                if (input.systemAction==GameplaySystemAction::none && playable && (ordinary||phase==GameplayPhase::nativeMenu) && !livePreferences && !presentation && !calibration && !attract && !recoveredSystemActions_.empty()) {
                    input.systemAction = recoveredSystemActions_.front();
                    recoveredSystemActions_.pop_front();
                }
                if ((!ordinary&&phase!=GameplayPhase::nativeMenu)||livePreferences||presentation||calibration||attract) { recoveredSystemActions_.clear(); }
                NameEntryInput text;
                if (playable && phase == GameplayPhase::highscoreNameEntry) {
                    text.characters = std::exchange(nameCharacters_, {});
                } else {
                    nameCharacters_.clear();
                }
                const bool pollingContinue = model.phase() == GameplayPhase::continuePrompt
                    && model.recoveredContinueDecision() == ContinueDecision::pending;
                const auto packet = pollingContinue ? recoveredContinuePacket(recoveredContinueScans_) : ContinueKeyboardPacket{};
                //03EE's parent continuation does not poll driving/fire keys.
                const bool titleReturn = playable
                    && (phase == GameplayPhase::endingReturn || phase == GameplayPhase::campaignEnded);
                if(titleReturn) {input={};input.joystick=joystick;}
                delivery = recovered_->advanceGameplay(input,packet,text);
                if (pollingContinue && !recoveredContinueScans_.empty()) { recoveredContinueScans_.pop_front(); }
                if (model.phase() != GameplayPhase::continuePrompt
                    || model.recoveredContinueDecision() != ContinueDecision::pending) { recoveredContinueScans_.clear(); }
                SetWindowTextW(window_, calibration
                    ? L"Stellar 7 | Joystick calibration | Enter/Space or button: Confirm"
                    : attract ? L"Stellar 7 | Attract presentation | Escape: Return"
                    : presentation
                    ? L"Stellar 7 | Story | Any key: Advance | Escape: Skip"
                    : recovered_->menuBackAvailable() && ordinary
                    ? L"Stellar 7 | Arrows: Select | Enter: Activate | Escape: Save and Play"
                    : model.phase() == GameplayPhase::nativeMenu
                    ? (sourceKeysEnabled()?L"Stellar 7 | Arrows: Select | Enter: Activate | Escape: Back (submenus) | Exit item":L"Stellar 7 | Arrows: Select | Enter: Activate | Escape: Back/Exit")
                    : model.phase() == GameplayPhase::continuePrompt
                        ? L"Stellar 7 | Up/Down: Continue choice | Enter/Space: Select | Escape: Exit"
                        : L"Stellar 7 | Arrows: Drive | Space: Fire | I/E/J/B/C/S/T: Systems | Tab: Select | Enter: Activate | Escape/F10: Menu");
            } else {
                NativeMenuInput input;
                if (recovered_->startup().phase() == NativeStartupPhase::awaitingMenuDecision||recovered_->startup().storyActive()||recovered_->joystickCalibrationActive()||recoveredAttract()) {
                    input = {recoveredHeld(VK_UP),
                        recoveredHeld(VK_DOWN),
                        recoveredHeld(VK_RETURN),
                        recoveredHeld(VK_LEFT),
                        recoveredHeld(VK_RIGHT),
                        (recovered_->menuBackAvailable()||recovered_->startup().storyActive()) && recoveredHeld(VK_ESCAPE)};
                    if(recovered_->startup().storyActive()) {
                        input.confirm=std::exchange(recoveredPresentationKey_,false)||input.confirm;
                        input.cancel=std::exchange(recoveredPresentationEscape_,false)||input.cancel;
                        if(input.cancel) {recoveredMenuEscapeConsumed_=true;}
                    } else {recoveredPresentationKey_=false;recoveredPresentationEscape_=false;}
                    if(!focused) {input={};}
                    if(recovered_->joystickCalibrationActive()) {
                        input.cancel=false;input.confirm=input.confirm||(focused&&(GetAsyncKeyState(VK_SPACE)&0x8000)!=0);
                    }
                    if(recovered_->startup().storyActive()&&recoveredIntroSkipRequested_) {
                        input.cancel=true;recoveredIntroSkipRequested_=false;recoveredMenuEscapeConsumed_=true;
                    }
                    input.pointer=recoveredPointer();
                    SetWindowTextW(window_, recovered_->joystickCalibrationActive()
                        ? L"Stellar 7 | Joystick calibration | Enter/Space or button: Confirm"
                        : recoveredAttract() ? L"Stellar 7 | Attract presentation | Escape: Return"
                        : recovered_->startup().storyActive()
                        ? L"Stellar 7 | Story | Any key: Advance | Escape: Skip"
                        : (sourceKeysEnabled()?L"Stellar 7 | Arrows: Select | Enter: Activate | Escape: Back (submenus) | Exit item":L"Stellar 7 | Arrows: Select | Enter: Activate | Escape: Back/Exit"));
                }
                input.joystick=joystick;input.sourceKey=sourceKey;
                input.calibrateJoystick=std::exchange(recoveredCalibrationRequested_,false);
                delivery = recovered_->advanceStartup(input,[&](const NativeSessionDelivery& chunk) {
                    if (!running) { return; }
                    running = queueRecovered(chunk);
                    streamedSamples += chunk.samples.size();
                });
            }
            finishSourceInput(sourceKey,menuPolled);
            if (!running || !deliverRecovered(delivery,streamedSamples) || delivery.exitRequested) { break; }
        }
        DestroyWindow(window_);
        window_ = nullptr;
        instance_ = nullptr;
        return 0;
    }

    bool presentStartup() {
        NativeStartupSequence startup(assets_, *startupAssets_);
        startup.start();
        startup.acceptSkipInput(); //The entry command explicitly selects this admitted route.
        presentFrame(startup.frame());
        auto previousPhase = startup.phase();
        auto previousLogoStep = startup.logoSteps();
        auto previousMenuSelection = MenuModel::itemCount;
        SetWindowTextW(window_, L"Stellar 7 native entry | intro skip selected | Escape exits");
        while (startup.phase() != NativeStartupPhase::ready) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    return false;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0) {
                return false;
            }
            if (startup.phase() == NativeStartupPhase::menu) {
                if (previousMenuSelection != startup.menu().selection()) {
                    SetWindowTextW(window_, L"Stellar 7 | Arrows: Select | Enter: Activate | Escape: Exit");
                    previousMenuSelection = startup.menu().selection();
                }
                if (smokeInputs_) {
                    startup.requestBegin();
                    ++smokeResult_.startupBeginRequests;
                } else {
                    const auto action = startup.pollMenu({(GetAsyncKeyState(VK_UP) & 0x8000) != 0,
                        (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0, (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0});
                    if (action == NativeMenuAction::exit) {
                        return false;
                    }
                    if (action == NativeMenuAction::begin) {
                        ++smokeResult_.startupBeginRequests;
                    } else if (action == NativeMenuAction::unsupported) {
                        SetWindowTextW(window_, L"Stellar 7 | Selected submenu is not implemented");
                    }
                }
            }
            if (startup.elapsedSamples() != audio_.queuedSamples()) {
                throw std::logic_error("native startup PCM cursor discontinuity");
            }
            constexpr std::size_t startupDeliverySamples = 512;
            if (!audio_.submit(startup.advance(startupDeliverySamples))) {
                return false;
            }
            if (startup.phase() != previousPhase || startup.logoSteps() != previousLogoStep
                || startup.phase() == NativeStartupPhase::menu) {
                presentFrame(startup.frame());
                previousPhase = startup.phase();
                previousLogoStep = startup.logoSteps();
            }
        }
        session_ = std::make_unique<NativeGameplaySession>(assets_, GameplayModel{},
            NativeSessionStart::level1, startup.releaseAudio(), options_.highscoreStore);
        return true;
    }

    void presentFrame(const IndexedScreen& indexed) {
        updatePixels(indexed);
        ++pixelsRevision_;
        if (!InvalidateRect(window_, nullptr, FALSE) || !UpdateWindow(window_)
            || paintFailed_ || paintedRevision_ != pixelsRevision_) {
            throw std::runtime_error("native frontend frame paint failed");
        }
    }

    bool presentDelivery(const NativeSessionDelivery& delivery) {
        if (delivery.firstSample != audio_.queuedSamples()) {
            throw std::logic_error("native frontend PCM cursor discontinuity");
        }
        const auto origin = std::chrono::steady_clock::now();
        for (const auto& entry : delivery.entry) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    return false;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0) {
                return false;
            }
            if (!audio_.submit(entry.samples)) {
                return false;
            }
            std::this_thread::sleep_until(origin + std::chrono::nanoseconds(entry.elapsedNanoseconds));
            presentFrame(entry.indexed);
        }
        return audio_.submit(delivery.samples);
    }

    void updatePixels(const IndexedScreen& indexed) {
        for (std::size_t offset = 0; offset < indexed.pixels.size(); ++offset) {
            const Rgb color = indexed.palette[indexed.pixels[offset]];
            bgra_[offset] = static_cast<std::uint32_t>(color.blue)
                | (static_cast<std::uint32_t>(color.green) << 8)
                | (static_cast<std::uint32_t>(color.red) << 16);
        }
    }

    static LRESULT CALLBACK windowProcedure(
        HWND window,
        UINT message,
        WPARAM wordParameter,
        LPARAM longParameter
    ) {
        if(instance_&&instance_->recovered_&&instance_->recovered_->execution()==CampaignExecution::playable) {
            instance_->recoveredKeys_.transition(message,wordParameter);
            if(message==WM_KILLFOCUS) {instance_->recoveredKeys_.clear();}
        }
        if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && instance_ != nullptr && instance_->recovered_) {
            if(instance_->sourceKeysEnabled()&&(instance_->recoveredMainMenu()||instance_->recoveredAttract()
                ||instance_->recovered_->presentationInputActive())) {
                instance_->recoveredSourceKeys_.keyDown(message,wordParameter,longParameter);
            }
            if(instance_->recoveredAttract()||(wordParameter==VK_ESCAPE&&instance_->sourceKeysEnabled()&&instance_->recoveredMainMenu())) {
                if(wordParameter==VK_ESCAPE) {instance_->recoveredMenuEscapeConsumed_=true;}
                return 0;
            }
            if(instance_->recovered_->joystickCalibrationActive()) {
                if(wordParameter==VK_ESCAPE) {instance_->recoveredMenuEscapeConsumed_=true;}
                return 0;
            }
            if(instance_->recovered_->presentationInputActive()) {
                if(wordParameter==VK_ESCAPE) {
                    instance_->recoveredPresentationEscape_=true;
                    instance_->recoveredMenuEscapeConsumed_=true;
                } else if(wordParameter!=VK_SHIFT&&wordParameter!=VK_CONTROL&&wordParameter!=VK_MENU) {
                    instance_->recoveredPresentationKey_=true;
                }
                return 0;
            }
            if(!instance_->recovered_->session()&&!instance_->recoveredMainMenu()
                &&instance_->recovered_->execution()==CampaignExecution::playable
                &&wordParameter!=VK_SHIFT&&wordParameter!=VK_CONTROL&&wordParameter!=VK_MENU) {
                instance_->recoveredIntroSkipRequested_=true;return 0;
            }
            if (wordParameter == VK_ESCAPE) {
                if(instance_->recovered_->execution()==CampaignExecution::playable) {return 0;}
                if (instance_->recoveredMenuEscapeConsumed_ || instance_->recovered_->menuBackAvailable()) {
                    instance_->recoveredMenuEscapeConsumed_ = true;
                } else { instance_->recoveredExitRequested_ = true; }
                return 0;
            }
            const auto* session = instance_->recovered_->session();
            const bool alt=(GetKeyState(VK_MENU)&0x8000)!=0;
            const bool control=(GetKeyState(VK_CONTROL)&0x8000)!=0;
            const bool firstPress=(longParameter&(1L<<30))==0;
            const bool calibrationFeature=hasNativeMenuFeature(instance_->options_.menuFeatures,NativeMenuFeatures::joystickControls);
            if(wordParameter=='C'&&alt&&!control&&firstPress&&calibrationFeature
                &&!(session&&session->model().phase()!=GameplayPhase::nativeMenu&&instance_->recovered_->menuBackAvailable())) {
                if(!instance_->options_.inputRecordingPath.empty()) {instance_->recoveredRecordingRejected_=true;return 0;}
                if(session) {instance_->recoveredSystemActions_.push_back(GameplaySystemAction::calibrateJoystick);}
                else if(instance_->recoveredMainMenu()) {instance_->recoveredCalibrationRequested_=true;}
                return 0;
            }
            if (session && instance_->recovered_->execution() == CampaignExecution::playable) {
                const auto phase = session->model().phase();
                const bool ordinary = phase == GameplayPhase::level1 || phase == GameplayPhase::level2
                    || phase == GameplayPhase::level3Native || phase == GameplayPhase::playable
                    || phase == GameplayPhase::level5;
                const bool modified = (GetKeyState(VK_CONTROL) & 0x8000) != 0
                    || (GetKeyState(VK_MENU) & 0x8000) != 0;
                if (ordinary && !modified) {
                    if(instance_->recovered_->menuBackAvailable()) {return 0;}
                    if (wordParameter == VK_F10 && (longParameter&(1L<<30))==0
                        &&hasNativeMenuFeature(instance_->options_.menuFeatures,NativeMenuFeatures::fullGraphicDetail)) {
                        instance_->recoveredSystemActions_.push_back(GameplaySystemAction::preferences);
                    } else if (wordParameter == 'I') {
                        instance_->recoveredSystemActions_.push_back(GameplaySystemAction::decoy);
                    } else if (wordParameter == 'B') {
                        instance_->recoveredSystemActions_.push_back(GameplaySystemAction::bomb);
                    } else if (wordParameter == 'J') {
                        instance_->recoveredSystemActions_.push_back(GameplaySystemAction::jump);
                    } else if (wordParameter == VK_RETURN) {
                        instance_->recoveredSystemActions_.push_back(GameplaySystemAction::activateSelected);
                    } else if (wordParameter == VK_TAB) {
                        instance_->recoveredSystemActions_.push_back(GameplaySystemAction::cycleSelected);
                    } else if (wordParameter == 'E') {
                        instance_->recoveredSystemActions_.push_back(GameplaySystemAction::shield);
                    } else if (wordParameter == 'C') {
                        instance_->recoveredSystemActions_.push_back(GameplaySystemAction::scanner);
                    } else if (wordParameter == 'S') {
                        instance_->recoveredSystemActions_.push_back(GameplaySystemAction::superPrimary);
                    } else if (wordParameter == 'T') {
                        instance_->recoveredSystemActions_.push_back(GameplaySystemAction::turbo);
                    }
                }
            }
            if (session && (session->model().phase() == GameplayPhase::failurePostExplosionBlackout
                || (session->model().phase() == GameplayPhase::continuePrompt
                    && session->model().recoveredContinueDecision() == ContinueDecision::pending))) {
                queueRecoveredContinueScan(message,longParameter,instance_->recoveredContinueScans_);
            }
        }
        if(message==WM_CHAR&&instance_&&instance_->recovered_&&instance_->sourceKeysEnabled()) {
            instance_->recoveredSourceKeys_.character(wordParameter,longParameter);
            if(instance_->recoveredAttract()||instance_->recoveredMainMenu()||instance_->recovered_->presentationInputActive()) {return 0;}
        }
        if(message==WM_SETCURSOR&&LOWORD(longParameter)==HTCLIENT&&instance_&&instance_->recovered_
            &&instance_->recovered_->execution()==CampaignExecution::playable) {SetCursor(nullptr);return TRUE;}
        if (message == WM_PAINT && instance_ != nullptr) {
            instance_->paint();
            return 0;
        }
        if (message == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        if (message == WM_CHAR && instance_ != nullptr
            && ((instance_->session_ && instance_->session_->model().phase() == GameplayPhase::highscoreNameEntry)
                || (instance_->recovered_ && instance_->recovered_->execution() == CampaignExecution::playable
                    && instance_->recovered_->session()
                    && instance_->recovered_->session()->model().phase() == GameplayPhase::highscoreNameEntry))) {
            if (wordParameter <= 0xff) {
                instance_->nameCharacters_.push_back(static_cast<std::uint8_t>(wordParameter));
            }
            return 0;
        }
        return DefWindowProcW(window, message, wordParameter, longParameter);
    }

    static inline Win32Gameplay* instance_ = nullptr;
    Win32HostKeys recoveredKeys_;
    bool recoveredPauseEscapeHeld_=false,recoveredIntroSkipRequested_=false;
    GameplayRunOptions options_;
    const std::vector<GameplayInput>* smokeInputs_ = nullptr;
    GameplaySmokeResult smokeResult_;
    std::uint64_t pixelsRevision_ = 0;
    std::uint64_t paintedRevision_ = 0;
    bool paintFailed_ = false;
    const GameplayAssets& assets_;
    const NativeStartupAssets* startupAssets_ = nullptr;
    std::unique_ptr<NativeGameplaySession> session_;
    Win32Joystick joystickHost_;
    std::unique_ptr<RecoveredFrontend> recovered_;
    std::deque<NativeTimedFrame> recoveredPresentations_;
    std::deque<std::uint8_t> recoveredContinueScans_;
    std::deque<GameplaySystemAction> recoveredSystemActions_;
    bool recoveredExitRequested_ = false;
    bool recoveredMenuEscapeConsumed_ = false;
    bool recoveredCalibrationRequested_=false;
    bool recoveredRecordingRejected_=false;
    Win32SourceKeys recoveredSourceKeys_;
    bool recoveredPresentationKey_ = false;
    bool recoveredPresentationEscape_ = false;
    Win32Audio audio_;
    GameplayInputRecorder inputRecorder_;
    std::vector<std::uint8_t> nameCharacters_;
    std::vector<std::uint32_t> bgra_;
    BITMAPINFO bitmapInfo_{};
    HWND window_ = nullptr;
};

}  // namespace

int runGameplay(
    const GameplayAssets& assets,
    const GameplayRunOptions& options
) {
    GameplayModel model;
    if (options.startAtLevel3) {
        model.startLevel3();
    } else if (options.startAtLevel2) {
        model.startLevel2();
    }
    return runGameplay(assets, std::move(model), options);
}

int runGameplay(
    const GameplayAssets& assets,
    GameplayModel initialModel,
    const GameplayRunOptions& options
) {
    return Win32Gameplay(assets, std::move(initialModel), options).run();
}

GameplaySmokeResult runGameplaySmoke(
    const GameplayAssets& assets,
    const std::vector<GameplayInput>& inputs
) {
    if (inputs.empty() || inputs.size() > maximumGameplaySmokeFrames) {
        throw std::invalid_argument("native frontend smoke requires 1..120 frames");
    }
    Win32Gameplay frontend(assets, GameplayModel{}, {}, &inputs);
    frontend.run();
    return frontend.smokeResult();
}

int runNativeEntry(const GameplayAssets& assets, const NativeStartupAssets& startup,
    const GameplayRunOptions& options) {
    return Win32Gameplay(assets, GameplayModel{}, options, nullptr, &startup).run();
}

int runRecoveredNativeEntry(const GameplayAssets& assets, const NativeStartupAssets& startup,
    const FrameCopyProfile copyProfile, const GameplayRunOptions& options) {
    if (copyProfile != FrameCopyProfile::modeXNoop || options.startAtLevel2 || options.startAtLevel3
        || !options.inputRecordingPath.empty()) {
        throw std::invalid_argument("unsupported recovered frontend entry profile");
    }
    return Win32Gameplay(assets, GameplayModel{}, options, nullptr, &startup, copyProfile).run();
}

int runPlayableNativeEntry(const GameplayAssets& assets, const NativeStartupAssets& startup,
    const FrameCopyProfile copyProfile, const GameplayRunOptions& options) {
    if (copyProfile != FrameCopyProfile::modeXNoop || options.startAtLevel2 || options.startAtLevel3
        || !options.inputRecordingPath.empty()) {
        throw std::invalid_argument("unsupported playable campaign entry profile");
    }
    return Win32Gameplay(assets, GameplayModel{}, options, nullptr, &startup, copyProfile,
        CampaignExecution::playable).run();
}

GameplaySmokeResult runNativeEntrySmoke(const GameplayAssets& assets,
    const NativeStartupAssets& startup, const std::vector<GameplayInput>& inputs) {
    if (inputs.empty() || inputs.size() > maximumNativeEntrySmokeFrames) {
        throw std::invalid_argument("native entry smoke requires 1..10000 frames");
    }
    Win32Gameplay frontend(assets, GameplayModel{}, {}, &inputs, &startup);
    frontend.run();
    return frontend.smokeResult();
}

}  // namespace stellar7
