#include "native_level2_frame_fixture.hpp"
#include "playable_campaign_pilot.hpp"
#include "playable_campaign_comparison.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>

namespace {
using namespace stellar7;
constexpr std::size_t noObject = static_cast<std::size_t>(-1);

void require(const bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<Model3d> models(const std::filesystem::path& root, const std::string& name) {
    std::ifstream file(root / name,std::ios::binary);
    require(static_cast<bool>(file), "native continuity resource is missing");
    return decodeFirstModelTableResource({std::istreambuf_iterator<char>(file),{}});
}

std::size_t find(const GameplayModel& model, const std::int16_t first, const std::int16_t last) {
    const auto& actors = model.level2Entities();
    const auto actor = std::find_if(actors.begin(), actors.end(), [first,last](const auto& value) {
        return value.active() && value.type >= first && value.type <= last;
    });
    return actor == actors.end() ? noObject : static_cast<std::size_t>(actor - actors.begin());
}

void route(const std::filesystem::path& resources) {
    const auto shared=models(resources,"COCKPIT.RES");
    for(std::uint8_t index=0;index<7;++index) {
        auto rng=StellarRng{};
        const auto local=models(resources,"LEVEL"+std::to_string(index+1)+".RES");
        const auto actors=constructCampaignLoaderActors(campaignLevelDefinition(index,0).table,rng,shared,local);
        const auto f3=std::count_if(actors.begin(),actors.end(),[](const auto& actor) {return actor.stateBytes4dTo52[4]==0xf3;});
        require(f3==(index==0 || index==3 ? 1 : 0),"actual constructor F3 domain changed");
    }
    std::cout<<"Actual constructor F3 domain: bank1 row5 and bank4 row6 only; banks2/7 negatives preserved\n";
    for(const std::uint8_t level:{0,3}) {
        const auto destination=static_cast<std::uint8_t>(level+3);
        const auto local=models(resources,"LEVEL"+std::to_string(destination+1)+".RES");
        auto session=test::makePlayableBlueWarpSession(resources,level);
        auto partitioned=test::makePlayableBlueWarpSession(resources,level);
        test::CampaignBudgetComparison composition;
        auto expectedSample=session->elapsedSamples();
        bool born=false,bluePalette=false,returned=false,loaded=false;
        unsigned nextFrames=0;
        const auto initialF3=std::count_if(session->model().level2Entities().begin(),session->model().level2Entities().end(),
            [](const auto& actor) {return actor.stateBytes4dTo52[4]==0xf3;});
        bool complete=false;
        for(unsigned call=0;call<1500;++call) {
            const auto before=session->model().phase();
            const auto beforeLevel=session->model().campaignLevel();
            std::optional<StellarRng> expectedLoaderRng;
            if(before==GameplayPhase::level2SuccessReturn) {
                require(session->model().pendingPlayableCampaignDestination()==destination,"actual blue26 lost source+3");
                returned=true;
                expectedLoaderRng=session->model().level2Rng();
                constructCampaignLoaderActors(campaignLevelDefinition(destination,0).table,*expectedLoaderRng,shared,local);
            }
            GameplayInput input{};
            if(before==GameplayPhase::playable && beforeLevel==level) {
                const auto blue=find(session->model(),26,26);
                if(blue!=noObject) {
                    input=test::campaignPilotControls(session->model(),session->model().level2Entities()[blue],test::CampaignPilotGoal::collect);
                }
            } else if(before==GameplayPhase::playable && beforeLevel==destination) {
                input.forward=input.left=input.fire=true;
            }
            try {
                const auto left=session->advancePlayableCampaign(input,10000);
                const auto right=partitioned->advancePlayableCampaign(input,257);
                const auto& model=session->model();
                const auto boundary=before!=model.phase() || beforeLevel!=model.campaignLevel() || call==0
                    || (before==GameplayPhase::playable && beforeLevel==destination && nextFrames==2);
                composition.compare(*session,*partitioned,left,right,boundary
                    ? test::CampaignComparisonStage::boundary : test::CampaignComparisonStage::frame);
                require(left.firstSample==expectedSample,"blue adapter reset retained PCM epoch");
                expectedSample+=left.samples.size();
                require(session->elapsedSamples()==expectedSample,"blue adapter lost PCM prefix");
                if(model.campaignLevel()==level) {
                    born=born || find(model,26,26)!=noObject;
                    if(model.phase()==GameplayPhase::level2SuccessPresentation) {
                        require(test::hasNativeBluePalette(*session),"actual blue26 contact selected a non-blue palette");
                        bluePalette=true;
                    }
                    if(born) {
                        require(std::count_if(model.level2Entities().begin(),model.level2Entities().end(),
                            [](const auto& actor) {return actor.stateBytes4dTo52[4]==0xf3;})==initialF3-1,
                            "blue26 birth did not retire its actual F3 producer");
                    }
                }
                if(expectedLoaderRng) {
                    require(model.campaignLevel()==destination && model.phase()==GameplayPhase::playableLoaderReturn,
                        "native blue+3 did not enter the actual destination loader");
                    test::compareCampaignRng(model.level2Rng(),*expectedLoaderRng,test::CampaignComparisonStage::boundary);loaded=true;
                }
                if(before==GameplayPhase::playable && beforeLevel==destination
                    && model.phase()==GameplayPhase::playable) {
                    ++nextFrames;
                }
                if(nextFrames==3) {
                    require(born && bluePalette && returned && loaded,"blue loader bypassed producer/contact/native success");
                    complete=true;
                }
                if(complete) {
                    std::cout<<"Declared actual F3 health1/one FD bank"<<unsigned(level+1)<<" -> actual26/bluecontact/+3 -> "
                        <<"bank"<<unsigned(destination+1)<<"+3"
                        <<" calls="<<call+1<<" budgets10000/257 samples="<<expectedSample<<'\n';
                    break;
                }
                require(model.playerArmor()>=0,"blue component route died before its warp");
            } catch(...) {
                std::cerr<<"blue sourcebank="<<unsigned(level+1)<<" call="<<call<<" beforephase="<<static_cast<int>(before)
                    <<" afterphase="<<static_cast<int>(session->model().phase())<<" born="<<born<<" palette="<<bluePalette<<'\n';
                throw;
            }
        }
        require(complete,"native blue component did not reach its bounded destination");
    }
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "expected original resource directory\n";
        return 2;
    }
    try {
        route(argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
