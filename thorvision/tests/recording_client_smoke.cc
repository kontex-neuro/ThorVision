#include "xdaqvc/recording.h"
#include <iostream>
#include <stdexcept>

int main() {
    xvc::RecordingClient client;
    auto initial=client.state();
    if(!initial)throw std::runtime_error(initial.error);
    auto revision=initial.state.at("revision").get<int>();
    auto original=initial.state.at("segment_seconds").get<int>();
    nlohmann::json configuration={{"revision",revision},{"segment_seconds",original==60?30:60},{"cameras",nlohmann::json::array()}};
    auto changed=client.apply(configuration);
    if(!changed || changed.state.at("revision").get<int>()!=revision+1)throw std::runtime_error(changed.error);
    auto stale=client.apply(configuration);
    if(stale.status!=409)throw std::runtime_error("Stale recording configuration was not rejected");
    auto repair=client.storage("repair",{{"uuid","unattached"},{"generation","stale"},{"confirmed",true}});
    if(repair.status!=409)throw std::runtime_error("Unqualified repair was not rejected");
    configuration["revision"]=changed.state.at("revision");configuration["segment_seconds"]=original;
    auto restored=client.apply(configuration);
    if(!restored)throw std::runtime_error(restored.error);
    std::cout<<"Cross-device recording API: snapshot, configuration, stale revision, repair gate and restoration passed\n";
}
