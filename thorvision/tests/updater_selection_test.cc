#include "DeviceUpdateSelection.h"
#include <iostream>
#include <stdexcept>
using namespace xvc::update;
using namespace thorvision::updates;
namespace {
int checks=0;
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); ++checks; }
Version version(const char* value) { return *Version::from_string(value); }
VersionEntry entry(const char* value,std::string product={}) {
    VersionEntry result;
    result.version=version(value); result.environment="release"; result.status="stable";
    result.platform="linux-arm64";result.product_id=product;
    result.profile=product=="aio"?"aio-jetson-orin-nx-l4t36.4.3-aarch64-cp310":"nvr-cm5-cp310";
    result.source_commit=std::string(40,'a');
    result.files.push_back(Artifact{.name="application.tar.xz",.size=1,.sha256=std::string(64,'b'),.role="application",.package_metadata_sha256=std::string(64,'c'),.minimum_updater="1.2.0"});
    result.files.push_back(Artifact{.name="migration.tar.xz",.size=1,.sha256=std::string(64,'d'),.role="updater-migration",.package_metadata_sha256=std::string(64,'e'),.minimum_updater="0.0.1"});
    return result;
}
DeviceDescriptor device(bool modern=false,std::string product={}) {
    DeviceDescriptor result;result.api_version=version("0.1.2");result.build_version=version("0.1.2");
    result.modern=modern;result.product_id=product;
    result.profile=modern?(product=="aio"?"aio-jetson-orin-nx-l4t36.4.3-aarch64-cp310":"nvr-cm5-cp310"):"";
    return result;
}
}
int main() {
 try {
    check(update_channel(std::nullopt).value()=="release","Absent process channel defaults to release");
    check(update_channel("release").value()=="release","Explicit release remains release");
    check(update_channel("dev").value()=="dev","Explicit DEV selects DEV");
    check(!update_channel(""),"Present empty process channel is refused");
    check(!update_channel("preview"),"Unknown process channel is refused");
    check(!update_channel("DEV"),"Channel spelling is exact, without implicit fallback");
    VersionMatrix matrix;matrix.versions.push_back(entry("0.1.3"));
    auto legacy=device();
    auto result=select(matrix,legacy,"","release",{0,1});
    check(result && std::holds_alternative<UpdatePlan>(*result),"Legacy catalog path must remain reachable when device_versions is empty");
    check(std::get<UpdatePlan>(*result).action==UpdateAction::Update && !std::get<UpdatePlan>(*result).required,"Legacy compatible patch update remains optional");
    auto parsed_legacy=parse_device_descriptor(R"({"version":"0.1.2"})",R"({"version":"0.1.2"})");
    check(parsed_legacy && !parsed_legacy->modern && parsed_legacy->product_id.empty() && parsed_legacy->profile.empty(),"Legacy discovery must not classify hardware as AIO");
    legacy.build_version=version("0.1.3");result=select(matrix,legacy,"","release",{0,1});
    check(result && std::get<UpdatePlan>(*result).action==UpdateAction::None,"Current legacy build is current without migration");
    result=select(matrix,legacy,"","release",{0,1},LegacyDeviceProduct::Unspecified,true);
    check(result && std::holds_alternative<ChooseDeviceType>(*result),"Manual factory check opens the product chooser even before initial public migration");
    legacy.build_version=version("0.0.5");legacy.api_version=version("0.0.2");result=select(matrix,legacy,"","release",{0,1});
    check(result && std::get<UpdatePlan>(*result).required,"Legacy incompatible series keeps its required update");
    legacy.build_version=version("0.1.3");legacy.api_version=version("0.2.0");result=select(matrix,legacy,"","release",{0,1});
    check(result && std::get<UpdatePlan>(*result).required && !api_compatible(legacy,{0,1}),"API mismatch cannot be hidden by compatible application version");
    legacy=device();legacy.build_version=version("0.1.4");auto recalled=entry("0.1.4");recalled.status="deprecated";matrix.versions.push_back(recalled);result=select(matrix,legacy,"","release",{0,1});
    check(result && std::get<UpdatePlan>(*result).target.version==version("0.1.3"),"Legacy recall keeps its reviewed fallback policy");
    legacy.build_version.reset();result=select(matrix,legacy,"","release",{0,1});
    check(!result,"Unknown legacy application build cannot choose an update implicitly");
    legacy=device();result=select(matrix,legacy,"","dev",{0,1});
    check(!result,"Legacy DEV does not silently imply AIO migration");
    matrix.device_versions.push_back(entry("0.1.6","aio"));auto aio=device(true,"aio");
    result=select(matrix,aio,"1.2.0","release",{0,1});
    check(result && std::holds_alternative<DeviceUpdatePlan>(*result) && std::get<DeviceUpdatePlan>(*result).target.product_id=="aio","Modern AIO uses product-aware selection");
    auto nvr=device(true,"nvr");result=select(matrix,nvr,"1.2.0","release",{0,1});
    check(!result,"NVR never falls back to AIO or the legacy ARM64 feed");
    aio.profile="aio-other-profile";result=select(matrix,aio,"1.2.0","release",{0,1});
    check(!result,"Wrong modern runtime profile is refused");
    legacy.build_version=version("0.1.3");
    result=select(matrix,legacy,"","release",{0,1});
    check(result && std::holds_alternative<ChooseDeviceType>(*result),"Factory connection offers a choice when a product release exists");
    result=select(matrix,legacy,"","release",{0,1},LegacyDeviceProduct::Nvr);
    check(!result,"NVR choice cannot use the AIO migration or the historical ARM64 feed");
    matrix.device_versions.push_back(entry("0.1.10","aio"));
    result=select(matrix,legacy,"","release",{0,1},LegacyDeviceProduct::Aio);
    check(result && std::get<DeviceUpdatePlan>(*result).migration_required &&
          std::get<DeviceUpdatePlan>(*result).target.version==version("0.1.10"),"AIO choice selects the newest complete release directly");
    result=select(matrix,device(true,"aio"),"1.2.1","release",{0,1},LegacyDeviceProduct::Nvr,true);
    check(result && std::get<DeviceUpdatePlan>(*result).target.product_id=="aio" &&
          !std::get<DeviceUpdatePlan>(*result).migration_required,"Recognized devices use actual identity despite a stale choice");
    aio=device(true,"aio");aio.api_version=version("0.2.0");result=select(matrix,aio,"1.2.0","release",{0,1});
    check(!result && result.error().code==ErrorCode::VersionNotFound,"Unsupported modern API reports no compatible update rather than malformed cloud data");
    auto dev_entry=entry("0.1.6","aio");dev_entry.environment="dev";
    VersionMatrix dev_matrix;dev_matrix.versions.push_back(entry("0.1.3"));
    dev_matrix.device_versions.push_back(dev_entry);
    auto dev_channel=update_channel("dev");legacy=device();
    result=select(dev_matrix,legacy,"",*dev_channel,{0,1},LegacyDeviceProduct::Aio);
    check(result && std::holds_alternative<DeviceUpdatePlan>(*result),"The confirmed AIO uses the DEV matrix");
    check(std::get<DeviceUpdatePlan>(*result).environment=="dev" && std::get<DeviceUpdatePlan>(*result).target.environment=="dev" && std::get<DeviceUpdatePlan>(*result).target.version==version("0.1.6") && std::get<DeviceUpdatePlan>(*result).legacy_aio_confirmation,"DEV AIO migration preserves channel, exact target and confirmation");
    result=select(dev_matrix,legacy,"",*dev_channel,{0,1});
    check(result && std::holds_alternative<ChooseDeviceType>(*result),"DEV factory devices also ask for the product type");
    result=select(dev_matrix,legacy,"","release",{0,1},LegacyDeviceProduct::Aio);
    check(!result,"Explicit release cannot silently consume a DEV-only device package");
    aio=device(true,"aio");result=select(dev_matrix,aio,"1.2.0",*dev_channel,{0,1});
    check(result && std::get<DeviceUpdatePlan>(*result).target.environment=="dev","Modern AIO uses explicit DEV channel");
    std::cout<<checks<<" updater selection checks passed\n";return 0;
 } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
