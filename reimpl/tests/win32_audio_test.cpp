#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <cassert>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdio>

namespace {
std::vector<WAVEHDR*> pending;
std::vector<std::int16_t> accepted;
std::vector<std::int16_t> acceptedPcm;
unsigned waits = 0;
bool quitPending = false;
std::uint64_t played = 0;
std::size_t maximumPendingSamples = 0;
unsigned positionCalls = 0;
MMRESULT openResult = MMSYSERR_NOERROR;
MMRESULT prepareResult = MMSYSERR_NOERROR;
MMRESULT writeResult = MMSYSERR_NOERROR;
std::array<SHORT,256> heldKeys{};
}

SHORT fakeKeyState(const int key) { return heldKeys.at(static_cast<std::size_t>(key)); }

MMRESULT fakeOpen(LPHWAVEOUT output, UINT, const WAVEFORMATEX*, DWORD_PTR, DWORD_PTR, DWORD) {
    *output = reinterpret_cast<HWAVEOUT>(1);
    played = 0;
    return openResult;
}
MMRESULT fakePrepare(HWAVEOUT, WAVEHDR* header, UINT) {
    if (prepareResult == MMSYSERR_NOERROR) { header->dwFlags |= WHDR_PREPARED; }
    return prepareResult;
}
MMRESULT fakeWrite(HWAVEOUT, WAVEHDR* header, UINT) {
    if (writeResult != MMSYSERR_NOERROR) { return writeResult; }
    header->dwFlags &= ~WHDR_DONE;
    pending.push_back(header);
    accepted.push_back(*reinterpret_cast<const std::int16_t*>(header->lpData));
    const auto* pcm = reinterpret_cast<const std::int16_t*>(header->lpData);
    acceptedPcm.insert(acceptedPcm.end(),pcm,pcm+header->dwBufferLength/sizeof(std::int16_t));
    std::size_t active = 0;
    for (const auto* item : pending) {
        if ((item->dwFlags & WHDR_DONE) == 0) { active += item->dwBufferLength / sizeof(std::int16_t); }
    }
    maximumPendingSamples = std::max(maximumPendingSamples,active);
    return MMSYSERR_NOERROR;
}
MMRESULT fakeUnprepare(HWAVEOUT, WAVEHDR* header, UINT) {
    if ((header->dwFlags & WHDR_DONE) == 0) { return WAVERR_STILLPLAYING; }
    pending.erase(std::remove(pending.begin(),pending.end(),header),pending.end());
    header->dwFlags &= ~WHDR_PREPARED;
    return MMSYSERR_NOERROR;
}
MMRESULT fakeReset(HWAVEOUT) {
    for (auto* header : pending) { header->dwFlags |= WHDR_DONE; }
    return MMSYSERR_NOERROR;
}
MMRESULT fakeClose(HWAVEOUT) { return MMSYSERR_NOERROR; }
void fakeSleep(DWORD) {
    ++waits;
    for (auto* header : pending) {
        if ((header->dwFlags & WHDR_DONE) == 0) {
            played += header->dwBufferLength / sizeof(std::int16_t);
            header->dwFlags |= WHDR_DONE;
            break;
        }
    }
}
BOOL fakePeek(LPMSG message, HWND, UINT, UINT, UINT) {
    if (!quitPending) { return FALSE; }
    quitPending = false;
    message->message = WM_QUIT;
    return TRUE;
}

MMRESULT fakePosition(HWAVEOUT, MMTIME* position, UINT) {
    ++positionCalls;
    position->wType = TIME_SAMPLES;
    position->u.sample = static_cast<DWORD>(played);
    return MMSYSERR_NOERROR;
}

#define waveOutGetPosition fakePosition
#define waveOutOpen fakeOpen
#define waveOutPrepareHeader fakePrepare
#define waveOutWrite fakeWrite
#define waveOutUnprepareHeader fakeUnprepare
#define waveOutReset fakeReset
#define waveOutClose fakeClose
#define Sleep fakeSleep
#define PeekMessageW fakePeek
#define GetAsyncKeyState fakeKeyState
#include "../src/frontend_win32.cpp"
#undef waveOutGetPosition
#undef waveOutOpen
#undef waveOutPrepareHeader
#undef waveOutWrite
#undef waveOutUnprepareHeader
#undef waveOutReset
#undef waveOutClose
#undef Sleep
#undef PeekMessageW
#undef GetAsyncKeyState

int main() {
    stellar7::Win32HostKeys keys;
    keys.transition(WM_KEYDOWN,VK_DOWN);keys.transition(WM_KEYDOWN,VK_DOWN);
    keys.transition(WM_KEYUP,VK_DOWN);
    keys.poll();assert(keys.held(VK_DOWN)&&keys.pressed(VK_DOWN));
    keys.poll();assert(!keys.held(VK_DOWN)&&!keys.pressed(VK_DOWN));
    keys.transition(WM_KEYDOWN,VK_RETURN);keys.transition(WM_KEYUP,VK_RETURN);
    keys.transition(WM_KEYDOWN,VK_RETURN);keys.transition(WM_KEYUP,VK_RETURN);
    keys.poll();assert(keys.held(VK_RETURN));keys.poll();assert(!keys.held(VK_RETURN));
    keys.poll();assert(keys.held(VK_RETURN));keys.poll();assert(!keys.held(VK_RETURN));
    keys.transition(WM_KEYDOWN,VK_ESCAPE);keys.poll();assert(keys.held(VK_ESCAPE));
    keys.clear();keys.poll();assert(!keys.held(VK_ESCAPE)&&!keys.pressed(VK_ESCAPE));
    std::deque<stellar7::NativeTimedFrame> frames{{10,{{1},{}}},{10,{{2},{}}},{20,{{3},{}}}};
    std::vector<std::uint8_t> presented;
    const auto publish=[&](const stellar7::IndexedScreen& frame) {presented.push_back(frame.pixels.front());};
    stellar7::presentRecoveredFrames(frames,9,publish);assert(presented.empty());
    stellar7::presentRecoveredFrames(frames,10,publish);assert((presented==std::vector<std::uint8_t>{1,2}));
    stellar7::presentRecoveredFrames(frames,19,publish);assert(presented.size()==2);
    stellar7::presentRecoveredFrames(frames,20,publish);assert((presented==std::vector<std::uint8_t>{1,2,3})&&frames.empty());
    std::deque<std::uint8_t> scans;
    stellar7::queueRecoveredContinueScan(WM_KEYDOWN,LPARAM{0x48} << 16,scans);
    stellar7::queueRecoveredContinueScan(WM_KEYDOWN,(LPARAM{0x48} << 16) | (LPARAM{1} << 30),scans);
    stellar7::queueRecoveredContinueScan(WM_KEYUP,LPARAM{0x48} << 16,scans);
    stellar7::queueRecoveredContinueScan(WM_KEYDOWN,LPARAM{1} << 16,scans);
    assert((scans == std::deque<std::uint8_t>{0x48,0x48}));
    constexpr std::array<int,8> sourceKeys{VK_END,VK_DOWN,VK_NEXT,VK_HOME,VK_UP,VK_PRIOR,VK_LEFT,VK_RIGHT};
    for (std::size_t index = 0; index < sourceKeys.size(); ++index) {
        heldKeys.fill(0);
        heldKeys[sourceKeys[index]] = static_cast<SHORT>(0x8000);
        heldKeys[VK_RETURN] = heldKeys[VK_SPACE] = static_cast<SHORT>(0x8000);
        const auto packet = stellar7::recoveredContinuePacket(scans);
        assert(packet.peekScan == 0x48 && packet.enter && packet.space && scans.size() == 2);
        for (std::size_t other = 0; other < sourceKeys.size(); ++other) { assert(packet.directions[other] == (index == other)); }
    }
    heldKeys.fill(0);
    scans.clear();
    const auto released = stellar7::recoveredContinuePacket(scans);
    assert(released.peekScan == 0 && !released.enter && !released.space);
    stellar7::Win32Audio audio;
    for (std::int16_t chunk = 1; chunk <= 9; ++chunk) { audio.submit({chunk, chunk}); }
    if (accepted.size() != 9) {
        std::fprintf(stderr, "PCM loss: accepted%zu of9 chunks after8 busy buffers\n", accepted.size());
        return 1;
    }
    for (std::size_t index = 0; index < accepted.size(); ++index) { assert(accepted[index] == index + 1); }
    assert(waits != 0);
    assert(audio.queuedSamples() == 18);
    const auto beforeImmediateWait = positionCalls;
    assert(audio.waitForInput() && positionCalls == beforeImmediateWait);
    assert(audio.drain());
    assert(audio.completedSamples() == audio.queuedSamples());
    {
        stellar7::Win32Audio cancelled;
        assert(cancelled.submit({7, 7}));
        quitPending = true;
        assert(!cancelled.drain());
        assert(cancelled.queuedSamples() == 2);
        assert(cancelled.completedSamples() == 0);
    }
    enum class FailureStage { open, prepare, write };
    pending.clear();
    acceptedPcm.clear();
    {
        stellar7::Win32Audio continuous(stellar7::Win32Audio::Transport::continuous);
        std::vector<std::int16_t> source(2110);
        for (std::size_t index = 0; index < source.size(); ++index) { source[index] = static_cast<std::int16_t>(index); }
        assert(continuous.submit({source.begin(),source.begin()+187}));
        assert(continuous.queuedSamples() == 187 && continuous.submittedSamples() == 0);
        assert(continuous.submit({source.begin()+187,source.begin()+987}));
        assert(continuous.queuedSamples() == 987 && continuous.submittedSamples() == 512);
        assert(continuous.submit({source.begin()+987,source.end()}));
        assert(continuous.queuedSamples() == source.size() && continuous.submittedSamples() == 2048);
        assert(continuous.drain());
        assert(acceptedPcm == source);
        assert(continuous.completedSamples() == source.size() && continuous.submittedSamples() == source.size());
    }
    pending.clear();
    acceptedPcm.clear();
    maximumPendingSamples = 0;
    {
        stellar7::Win32Audio continuous(stellar7::Win32Audio::Transport::continuous);
        std::vector<std::int16_t> source(8001);
        for (std::size_t index = 0; index < source.size(); ++index) { source[index] = static_cast<std::int16_t>(index); }
        constexpr std::array<std::size_t,5> chunks{1,511,73,900,257};
        for (std::size_t offset = 0, chunk = 0; offset < source.size(); ++chunk) {
            const auto count = std::min(chunks[chunk % chunks.size()],source.size()-offset);
            assert(continuous.submit({source.begin()+offset,source.begin()+offset+count}));
            offset += count;
        }
        assert(maximumPendingSamples <= 8*512);
        assert(continuous.queuedSamples() == source.size());
        unsigned services = 0;
        continuous.setHostService([&] { ++services; return true; });
        assert(continuous.queuedSamples()-played > 2560);
        assert(continuous.waitForInput());
        assert(services != 0 && continuous.queuedSamples()-played <= 2560);
        assert(continuous.queuedSamples() == source.size());
        assert(continuous.drain());
        assert(acceptedPcm == source);
        assert(continuous.completedSamples() == source.size());
        assert(continuous.submittedSamples() == source.size());
    }
    pending.clear();
    {
        stellar7::Win32Audio continuous(stellar7::Win32Audio::Transport::continuous);
        assert(continuous.submit(std::vector<std::int16_t>(4096,17)));
        const auto queued = continuous.queuedSamples();
        unsigned services = 0;
        continuous.setHostService([&] { ++services; return false; });
        assert(queued-played > 2560);
        assert(!continuous.waitForInput());
        assert(services != 0 && continuous.queuedSamples() == queued);
        continuous.setHostService({});
        assert(continuous.drain());
    }
    for (const auto kind : {FailureStage::open, FailureStage::prepare, FailureStage::write}) {
        pending.clear();
        accepted.clear();
        openResult = kind == FailureStage::open ? MMSYSERR_ERROR : MMSYSERR_NOERROR;
        prepareResult = kind == FailureStage::prepare ? MMSYSERR_ERROR : MMSYSERR_NOERROR;
        writeResult = kind == FailureStage::write ? MMSYSERR_ERROR : MMSYSERR_NOERROR;
        bool rejected = false;
        try {
            stellar7::Win32Audio failed;
            failed.submit({1, 2});
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
    return 0;
}
