#include "stellar7/scene_script.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {

constexpr std::uint16_t maximumScenePages = 300;
constexpr std::size_t rectangleCapacity = 0x320 / 8;
constexpr CockpitCopyRegion screen{0, 0, 320, 200};

std::int16_t checkedWord(const int value) {
    if (value < std::numeric_limits<std::int16_t>::min()
        || value > std::numeric_limits<std::int16_t>::max()) {
        throw std::invalid_argument("scene rectangle requires non-wrapped signed arithmetic");
    }
    return static_cast<std::int16_t>(value);
}

std::uint16_t readWord(const std::vector<std::uint8_t>& bytes, std::size_t& offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::invalid_argument("truncated scene command word");
    }
    const auto value = static_cast<std::uint16_t>(bytes[offset]
        | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8));
    offset += 2;
    return value;
}

bool supported(const SceneCommandKind kind) {
    switch (kind) {
    case SceneCommandKind::cacheScreen:
    case SceneCommandKind::releaseBitmapBank:
    case SceneCommandKind::selectCaptureBank:
    case SceneCommandKind::captureRectangle:
    case SceneCommandKind::drawPoint:
    case SceneCommandKind::drawLine:
    case SceneCommandKind::fillRectangle:
    case SceneCommandKind::drawMirroredBitmap:
    case SceneCommandKind::restoreCapture:
    case SceneCommandKind::text1:
    case SceneCommandKind::text4:
    case SceneCommandKind::text5:
    case SceneCommandKind::text6:
    case SceneCommandKind::endPage:
    case SceneCommandKind::setDelay:
    case SceneCommandKind::selectFrame:
    case SceneCommandKind::selectBitmapBank:
    case SceneCommandKind::selectPaletteBank:
    case SceneCommandKind::selectFontBank:
    case SceneCommandKind::metadata1111:
    case SceneCommandKind::metadata1201:
    case SceneCommandKind::setColors:
    case SceneCommandKind::sound:
    case SceneCommandKind::cacheRectangle:
    case SceneCommandKind::dialogue:
    case SceneCommandKind::drawBitmap:
    case SceneCommandKind::loadBackground:
    case SceneCommandKind::loadBitmaps:
    case SceneCommandKind::bindFont:
    case SceneCommandKind::loadPalette:
    case SceneCommandKind::text0:
    case SceneCommandKind::text2:
    case SceneCommandKind::text3:
        return true;
    }
    return false;
}

} // namespace

SceneProgram decodeSceneProgram(const std::vector<std::uint8_t>& payload,
    const std::uint16_t declaredPageCount) {
    if (declaredPageCount == 0 || declaredPageCount > maximumScenePages) {
        throw std::invalid_argument("scene page count outside source table");
    }
    SceneProgram program;
    std::vector<SceneCommand> page;
    std::size_t offset = 0;
    while (offset < payload.size()) {
        const auto sourceOffset = offset;
        const auto opcode = readWord(payload, offset);
        const auto kind = static_cast<SceneCommandKind>(opcode);
        if (!supported(kind)) {
            throw std::invalid_argument("unsupported scene command");
        }
        SceneCommand command{kind, {}, {}, sourceOffset};
        const auto argumentCount = opcode & 0x0fU;
        if (argumentCount == 0x0fU) {
            while (offset < payload.size() && payload[offset] != 0) {
                command.text.push_back(static_cast<char>(payload[offset++]));
            }
            if (offset == payload.size()) {
                throw std::invalid_argument("unterminated scene command string");
            }
            ++offset;
            if ((offset & 1U) != 0U) {
                if (offset == payload.size()) {
                    throw std::invalid_argument("truncated scene string alignment");
                }
                ++offset;
            }
        } else {
            for (std::size_t index = 0; index < argumentCount; ++index) {
                command.arguments[index] = readWord(payload, offset);
            }
        }
        page.push_back(std::move(command));
        if (kind != SceneCommandKind::endPage) {
            continue;
        }
        if (program.pages.size() == declaredPageCount) {
            throw std::invalid_argument("scene contains more pages than declared");
        }
        program.pages.push_back(std::move(page));
        page.clear();
    }
    if (!page.empty() || program.pages.size() != declaredPageCount) {
        throw std::invalid_argument("scene page terminators disagree with PAG record");
    }
    return program;
}

ScenePageInterpreter::ScenePageInterpreter(const SceneResource& resource, const RadarPages pages,
    const std::uint8_t primary, const std::uint8_t secondary, const SceneResource* sharedAssets)
    : resource_(&resource), assets_(sharedAssets ? sharedAssets : &resource), pages_(pages), primary_(primary), secondary_(secondary) {
    std::array<std::uint16_t, 3> roles{pages.cached, pages.back, pages.front};
    std::sort(roles.begin(), roles.end());
    if (roles != std::array<std::uint16_t, 3>{0xa000, 0xa400, 0xa800}) {
        throw std::invalid_argument("scene requires three distinct native Mode-X page roles");
    }
}

void ScenePageInterpreter::appendCopy(ScenePageWork& work, const CockpitCopyRegion rectangle,
    const std::uint16_t source, const std::uint16_t destination) const {
    if (rectangle.x < 0 || rectangle.y < 0 || rectangle.width <= 0 || rectangle.height <= 0
        || rectangle.x + rectangle.width > screen.width
        || rectangle.y + rectangle.height > screen.height) {
        throw std::invalid_argument("scene copy rectangle outside display");
    }
    constexpr std::uint16_t pixelsPerPlanarByte=4;
    const auto alignment=rectangle.x%pixelsPerPlanarByte;
    const auto groups=(rectangle.width+2*alignment+pixelsPerPlanarByte-1)/pixelsPerPlanarByte;
    const auto width=static_cast<std::uint16_t>(pixelsPerPlanarByte*groups);
    const auto left=static_cast<std::int16_t>(rectangle.x-alignment);
    const RadarPageCopy copy{source,destination,{left,rectangle.y},width,static_cast<std::uint16_t>(rectangle.height)};
    if(left+width>screen.width) {
        //Loaded106A/6742 executes REP MOVSB without a right-edge clamp. The
        //padded latch group aliases the next raster row, or retained page padding.
        work.operations.emplace_back(ScenePlanarCopy{copy});
    } else {
        planarCopyWork(rectangle.x,rectangle.width,rectangle.height);
        work.operations.emplace_back(copy);
    }
}

void ScenePageInterpreter::enqueue(CockpitCopyRegion rectangle) {
    if (rectangle.x < 0) {
        rectangle.width = checkedWord(rectangle.width + rectangle.x);
        rectangle.x = 0;
    }
    if (rectangle.y < 0) {
        rectangle.height = checkedWord(rectangle.height + rectangle.y);
        rectangle.y = 0;
    }
    if (checkedWord(rectangle.x + rectangle.width) > screen.width) {
        rectangle.width = checkedWord(screen.width - rectangle.x);
    }
    if (checkedWord(rectangle.y + rectangle.height) > screen.height) {
        rectangle.height = checkedWord(screen.height - rectangle.y);
    }
    if (rectangle.width == 0 || rectangle.height == 0) {
        return;
    }
    if (rectangle.width < 0 || rectangle.height < 0 || rectangle.x >= screen.width
        || rectangle.y >= screen.height || current_.size() == rectangleCapacity) {
        throw std::invalid_argument("unsupported scene dirty rectangle or queue capacity");
    }
    current_.push_back(rectangle);
}

void ScenePageInterpreter::commitCache(ScenePageWork& work, const CockpitCopyRegion rectangle) {
    appendCopy(work, rectangle, pages_.back, pages_.cached);
    if (current_.size() + previous_.size() > rectangleCapacity) {
        throw std::invalid_argument("scene cache commit exceeds previous rectangle queue");
    }
    while (!current_.empty()) {
        previous_.push_back(current_.back());
        current_.pop_back();
    }
}

std::size_t ScenePageInterpreter::consumeAsset(const std::size_t variantIndex) {
    if (assetCursor_ == assets_->assets.size()
        || assets_->assets.at(assetCursor_).index() != variantIndex) {
        throw std::invalid_argument("scene embedded resource cursor has the wrong record type");
    }
    return assetCursor_++;
}

ScenePageWork ScenePageInterpreter::execute(const std::size_t pageIndex) {
    // Qualification is transactional: rejected work cannot alter the retained VM.
    auto candidate = *this;
    auto work = candidate.executePage(pageIndex);
    *this = std::move(candidate);
    return work;
}

ScenePageWork ScenePageInterpreter::executeComposite(const std::vector<std::size_t>& pages) {
    if (pages.empty()) { throw std::invalid_argument("scene composite requires a page"); }
    auto candidate = *this;
    ScenePageWork result{};
    for (std::size_t index = 0; index < pages.size(); ++index) {
        ScenePageWork work;
        try {work=candidate.executePage(pages[index],index==0,index+1==pages.size() && pages[index]<resource_->program.pages.size());}
        catch(const std::exception& error) {throw std::invalid_argument("scene composite page "+std::to_string(pages[index])+": "+error.what());}
        result.present = work.present;
        result.delayIrqCounts = work.delayIrqCounts;
        result.operations.insert(result.operations.end(),work.operations.begin(),work.operations.end());
    }
    *this = std::move(candidate);
    return result;
}

ScenePageWork ScenePageInterpreter::executePage(const std::size_t pageIndex, const bool begin, const bool present) {
    ScenePageWork work{pageIndex < resource_->program.pages.size(), {}, 0};
    if (begin) {
        for (const auto rectangle : current_) { appendCopy(work, rectangle, pages_.cached, pages_.back); }
        previous_ = std::move(current_);
        current_.clear();
    }
    if (work.present) {
        const auto& commands = resource_->program.pages.at(pageIndex);
        if (commands.empty() || commands.back().kind != SceneCommandKind::endPage) {
            throw std::invalid_argument("scene page lacks its source terminator");
        }
        for (const auto& command : commands) {
            const auto& args = command.arguments;
            const CockpitCopyRegion rectangle{static_cast<std::int16_t>(args[0]),
                static_cast<std::int16_t>(args[1]), static_cast<std::int16_t>(args[2]),
                static_cast<std::int16_t>(args[3])};
            switch (command.kind) {
            case SceneCommandKind::selectBitmapBank:
                if (args[0] >= bitmapBanks_.size()) { throw std::invalid_argument("scene bitmap bank outside source table"); }
                bitmapBank_ = args[0]; bitmaps_ = bitmapBanks_[bitmapBank_];
                break;
            case SceneCommandKind::releaseBitmapBank:
                bitmapBanks_[bitmapBank_].reset(); bitmaps_.reset();
                break;
            case SceneCommandKind::selectCaptureBank:
                if (args[0] >= captures_.size()) { throw std::invalid_argument("scene capture bank outside source table"); }
                captureBank_ = args[0];
                break;
            case SceneCommandKind::captureRectangle:
                if (rectangle.x < 0 || rectangle.y < 0 || rectangle.width <= 0 || rectangle.height <= 0
                    || rectangle.x + rectangle.width > 320 || rectangle.y + rectangle.height > 200
                    || rectangle.x % 4 != 0 || rectangle.width % 4 != 0) {
                    throw std::invalid_argument("scene capture outside source aligned screen");
                }
                captures_[captureBank_] = rectangle;
                work.operations.emplace_back(SceneCapture{captureBank_,rectangle});
                break;
            case SceneCommandKind::restoreCapture:
                if (args[0] >= captures_.size() || !captures_[args[0]]) {
                    throw std::invalid_argument("scene restore lacks retained capture");
                }
                enqueue(*captures_[args[0]]);
                work.operations.emplace_back(SceneRestoreCapture{args[0],*captures_[args[0]]});
                break;
            case SceneCommandKind::cacheScreen:
                commitCache(work,screen); fullDirty_ = true;
                break;
            case SceneCommandKind::drawPoint:
                enqueue({rectangle.x,rectangle.y,8,1});
                work.operations.emplace_back(ScenePrimitiveDraw{command.kind,rectangle,primary_});
                break;
            case SceneCommandKind::drawLine:
                if (rectangle.x != rectangle.width && rectangle.y != rectangle.height) {
                    throw std::invalid_argument("story line requires its axis-aligned source domain");
                }
                enqueue({std::min(rectangle.x,rectangle.width),std::min(rectangle.y,rectangle.height),
                    checkedWord(std::abs(int(rectangle.x)-rectangle.width)+8),
                    checkedWord(std::abs(int(rectangle.y)-rectangle.height)+2)});
                work.operations.emplace_back(ScenePrimitiveDraw{command.kind,rectangle,primary_});
                break;
            case SceneCommandKind::fillRectangle:
                enqueue({rectangle.x,rectangle.y,checkedWord(rectangle.width+8),checkedWord(rectangle.height+2)});
                work.operations.emplace_back(ScenePrimitiveDraw{command.kind,rectangle,primary_});
                break;
            case SceneCommandKind::selectFontBank:
                if (args[0] != 0) {
                    throw std::invalid_argument("scene requires its admitted resource bank zero");
                }
                break;
            case SceneCommandKind::selectPaletteBank:
                if (args[0] != 0) {
                    throw std::invalid_argument("scene requires palette bank zero");
                }
                work.operations.emplace_back(ScenePaletteChange{ScenePaletteOperation::selectBank, palette_});
                break;
            case SceneCommandKind::loadPalette:
                palette_ = consumeAsset(0);
                work.operations.emplace_back(ScenePaletteChange{ScenePaletteOperation::load, palette_});
                break;
            case SceneCommandKind::loadBitmaps:
                bitmaps_ = consumeAsset(1);
                bitmapBanks_[bitmapBank_] = bitmaps_;
                selectedFrame_.reset();
                break;
            case SceneCommandKind::loadBackground: {
                const auto asset = consumeAsset(2);
                if (std::get<2>(assets_->assets.at(asset)).size() != 320U * 200U) {
                    throw std::invalid_argument("scene background is not a full indexed screen");
                }
                work.operations.emplace_back(SceneBackgroundDraw{asset});
                commitCache(work, screen);
                fullDirty_ = true;
                break;
            }
            case SceneCommandKind::bindFont:
                // F04F's logical filename is ignored; the draw owner binds12CC.
                break;
            case SceneCommandKind::selectFrame:
                if (!bitmaps_ || args[0] >= std::get<1>(assets_->assets.at(*bitmaps_)).size()) {
                    throw std::invalid_argument("scene frame selection outside loaded bitmap bank");
                }
                selectedFrame_ = args[0];
                break;
            case SceneCommandKind::drawBitmap:
            case SceneCommandKind::drawMirroredBitmap:
            case SceneCommandKind::dialogue:
                enqueue({rectangle.x, rectangle.y, checkedWord(rectangle.width + 8),
                    checkedWord(rectangle.height + 2)});
                if (command.kind == SceneCommandKind::drawBitmap || command.kind == SceneCommandKind::drawMirroredBitmap) {
                    if (!bitmaps_ || !selectedFrame_) {
                        throw std::invalid_argument("scene bitmap draw lacks selected frame");
                    }
                    work.operations.emplace_back(SceneBitmapDraw{*bitmaps_, *selectedFrame_,
                        {rectangle.x, rectangle.y}, command.kind == SceneCommandKind::drawMirroredBitmap});
                } else {
                    work.operations.emplace_back(SceneDialogueDraw{rectangle, text_, primary_, secondary_});
                }
                break;
            case SceneCommandKind::cacheRectangle:
                commitCache(work, rectangle);
                break;
            case SceneCommandKind::setDelay:
                delay_ = args[0];
                break;
            case SceneCommandKind::setColors:
                primary_ = static_cast<std::uint8_t>(args[0]);
                secondary_ = static_cast<std::uint8_t>(args[1]);
                break;
            case SceneCommandKind::sound:
                work.operations.emplace_back(SceneSoundRequest{args[1] != 0
                    ? SceneSoundOperation::loadVoice : SceneSoundOperation::playSequence,
                    args[1] != 0 ? args[1] : args[0]});
                break;
            case SceneCommandKind::text0:
            case SceneCommandKind::text1:
            case SceneCommandKind::text2:
            case SceneCommandKind::text3:
            case SceneCommandKind::text4:
            case SceneCommandKind::text5:
            case SceneCommandKind::text6: {
                const auto slot = (static_cast<std::uint16_t>(command.kind) - 0xf10fU) >> 4;
                text_[slot] = command.text;
                for (std::size_t index = slot + 1; index < text_.size(); ++index) {
                    text_[index].clear();
                }
                break;
            }
            case SceneCommandKind::metadata1111:
            case SceneCommandKind::metadata1201:
                break;
            case SceneCommandKind::endPage:
                if (&command != &commands.back()) {
                    throw std::invalid_argument("scene terminator precedes the end of its page");
                }
                if (fullDirty_) {
                    appendCopy(work, screen, pages_.back, pages_.front);
                    fullDirty_ = false;
                }
                break;
            default:
                throw std::invalid_argument("unsupported scene execution command");
            }
        }
    }
    if (present) {
        for (const auto rectangle : current_) { appendCopy(work, rectangle, pages_.back, pages_.front); }
        for (const auto rectangle : previous_) { appendCopy(work, rectangle, pages_.back, pages_.front); }
        previous_.clear();
    }
    work.delayIrqCounts = static_cast<std::uint16_t>(delay_ * 4U);
    return work;
}

} // namespace stellar7
