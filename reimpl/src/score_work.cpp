#include "stellar7/score_work.hpp"
#include "stellar7/indexed_render.hpp"
#include "stellar7/render_work.hpp"
#include "stellar7/ui_fill_work.hpp"
#include "stellar7/ui_text_work.hpp"

#include <limits>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::int16_t ordinaryTankAward = 20;
constexpr std::int16_t ordinaryLiftAward = 50;
constexpr std::int16_t level2TankAward = 25;
constexpr std::int16_t level2Type4Award = 30;
constexpr std::int16_t level2LiftAward = 10;
constexpr std::int16_t systemBonusAward = 100;
constexpr std::int16_t guardianAward = 1000;
constexpr std::uint8_t scoreColor = 239;
constexpr CockpitCopyRegion scoreRectangle{240,155,30,6};
constexpr Viewport display{0,319,0,199};

void decimalOperations(std::vector<NormalWorkOperation>& operations, const std::uint32_t score) {
    const auto plain = [&](const std::uint64_t count) {
        operations.push_back({NormalWorkKind::instructions,count});
    };
    //Positive signed32 divided by positive10: E23C pointer wrapper/E0D0 leaf.
    const auto divide = [&]() {
        plain(9); /*E23C..E24E.*/
        plain(9); /*E0D0..E0DD.*/
        plain(3); /*E0F0..E0F5.*/
        plain(11); /*E108..E11F.*/
        plain(2); /*E159..E15A.*/
        plain(5); /*E163..E167.*/
        plain(5); /*E251..E258.*/
    };
    const auto remainder = [&]() {
        plain(8); /*E19C..E1A8.*/
        plain(3); /*E1BB..E1C0.*/
        plain(13); /*E1D2..E1EC.*/
        plain(4); /*E236..E239.*/
    };
    plain(9); /*7A1C..7A2B.*/
    plain(3); /*7A3E..7A44.*/
    plain(7); /*7A4E..7A5E.*/
    if ((score >> 16) == 0) { plain(2); /*7A60..7A62.*/ }
    plain(1); /*7A64..7A64.*/
    for (auto remaining = score; remaining != 0;) {
        plain(8); /*7A67..7A72.*/
        divide();
        remaining /= 10;
        plain(2); /*7A75..7A77.*/
        if ((remaining >> 16) == 0) { plain(3); /*7A79..7A7D.*/ }
    }
    plain(10); /*7A7F..7A99.*/
    if ((score >> 16) == 0) { plain(2); /*7A9B..7A9D.*/ }
    plain(1); /*7A9F..7A9F.*/
    for (auto remaining = score; remaining != 0;) {
        plain(7); /*7AA2..7AAE.*/
        remainder();
        plain(12); /*7AB1..7AC6.*/
        divide();
        remaining /= 10;
        plain(2); /*7AC9..7ACB.*/
        if ((remaining >> 16) == 0) { plain(3); /*7ACD..7AD1.*/ }
    }
    plain(6); /*7AD3..7ADB.*/
}
}

ScoreWork scoreOperations(const std::uint32_t incomingScore, const std::int16_t award,
    const std::uint16_t difficulty, const BitmapFont& scoreFont, const CockpitPageState& pages,
    const ScoreAwardDomain domain) {
    const auto initialOrdinary = domain == ScoreAwardDomain::initialOrdinary;
    const auto systemBonus = domain == ScoreAwardDomain::systemBonus;
    const auto guardian = domain == ScoreAwardDomain::guardianDefeat;
    const auto transition = domain == ScoreAwardDomain::transitionDisplay;
    const auto periodic = domain == ScoreAwardDomain::level2PeriodicOrdinary;
    const auto level2 = domain == ScoreAwardDomain::level2Constructor || periodic;
    if ((domain != ScoreAwardDomain::ordinaryTank && !initialOrdinary && !systemBonus && !guardian && !transition && !level2)
        || (transition ? award != 0 || incomingScore<1000 : guardian ? award != guardianAward : systemBonus ? award != systemBonusAward
            : level2 ? award != level2TankAward && award != level2LiftAward
                && !(periodic && (award == ordinaryTankAward || award == level2Type4Award))
            : award != ordinaryTankAward && !(initialOrdinary && award == ordinaryLiftAward)) || difficulty != 0
        || incomingScore > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
            - static_cast<std::uint32_t>(award)
        || pages.cachePage() != 0xa400
        || !((pages.frontPage() == 0xa000 && pages.backPage() == 0xa800)
            || (pages.frontPage() == 0xa800 && pages.backPage() == 0xa000))) {
        throw std::invalid_argument("unsupported ordinary Type1 score transaction");
    }
    const auto score = incomingScore + static_cast<std::uint32_t>(award);
    //Independent semantic provider validates the exact score font profile.
    scorePageEffects(score,scoreFont,pages.backPage(),pages.frontPage(),CockpitPageLayout::recoveredStartup);
    ScoreWork result{score,pages,pages.backPage(),pages.frontPage(),scoreColor,std::to_string(score)+' ',{},{},{}};
    const auto plain = [&](const std::uint64_t count) {
        result.operations.push_back({NormalWorkKind::instructions,count});
    };
    const auto append = [&](const auto& work) {
        result.operations.insert(result.operations.end(),work.operations.begin(),work.operations.end());
        result.effects.insert(result.effects.end(),work.effects.begin(),work.effects.end());
    };
    plain(20); /*7970..799D.*/
    result.fontSelections.push_back({result.operations.size(),ScoreFontSelection::score});
    plain(16); /*79A2..79C6.*/
    //79A5..79AB sets DRAW=COPY=BACK; fill and text both write physicalBACK.
    append(uiFillOperations(scoreRectangle,0,pages.backPage()));
    plain(8); /*79CB..79DE.*/
    decimalOperations(result.operations,score);
    plain(3); /*79E1..79E5.*/
    append(uiTextOperations(scoreFont,UiFontLayout::proportional,result.text,
        {scoreRectangle.x,scoreRectangle.y},scoreColor,pages.backPage(),display));
    plain(12); /*79EA..7A03.*/
    //79ED..79F0 changes COPY to FRONT, leaving DRAW=BACK as the copy source.
    plain(1); //22A1 installed106A JMPF.
    const auto copy = planarCopyOperations(scoreRectangle.x,scoreRectangle.width,scoreRectangle.height);
    result.operations.insert(result.operations.end(),copy.begin(),copy.end());
    result.effects.emplace_back(RadarPageCopy{pages.backPage(),pages.frontPage(),
        {scoreRectangle.x,scoreRectangle.y},32,static_cast<std::uint16_t>(scoreRectangle.height)});
    plain(3); /*7A08..7A0F.*/
    result.fontSelections.push_back({result.operations.size(),ScoreFontSelection::primary});
    plain(5); /*7A14..7A1B.*/
    return result;
}

} // namespace stellar7
