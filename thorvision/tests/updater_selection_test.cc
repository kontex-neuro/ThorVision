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
    result.files.push_back(Artifact{.name="application.tar.xz",.size=1,.sha256=std::string(64,'b'),.role="application",.package_metadata_sha256=std::string(64,'c'),.minimum_updater="1.1.0"});
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
    VersionMatrix matrix;matrix.versions.push_back(entry("0.1.3"));
    auto legacy=device();
    auto result=select(matrix,legacy,"","release",{0,1});
    check(result && std::holds_alternative<UpdatePlan>(*result),"Legacy catalog path must remain reachable when device_versions is empty");
    check(std::get<UpdatePlan>(*result).action==UpdateAction::Update && !std::get<UpdatePlan>(*result).required,"Legacy compatible patch update remains optional");
    check(legacy.product_id.empty() && legacy.profile.empty(),"Legacy discovery must not classify hardware as AIO");
    legacy.build_version=version("0.1.3");result=select(matrix,legacy,"","release",{0,1});
    check(result && std::get<UpdatePlan>(*result).action==UpdateAction::None,"Current legacy build is current without migration");
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
    matrix.device_versions.push_back(entry("0.1.5","aio"));auto aio=device(true,"aio");
    result=select(matrix,aio,"1.1.0","release",{0,1});
    check(result && std::holds_alternative<DeviceUpdatePlan>(*result) && std::get<DeviceUpdatePlan>(*result).target.product_id=="aio","Modern AIO uses product-aware selection");
    auto nvr=device(true,"nvr");result=select(matrix,nvr,"1.1.0","release",{0,1});
    check(!result,"NVR never falls back to AIO or the legacy ARM64 feed");
    aio.profile="aio-other-profile";result=select(matrix,aio,"1.1.0","release",{0,1});
    check(!result,"Wrong modern runtime profile is refused");
    result=select(matrix,legacy,"","release",{0,1},true);
    check(result && std::holds_alternative<DeviceUpdatePlan>(*result) && std::get<DeviceUpdatePlan>(*result).legacy_aio_confirmation,"AIO migration is available only through explicit opt-in and confirmation");
    aio=device(true,"aio");aio.api_version=version("0.2.0");result=select(matrix,aio,"1.1.0","release",{0,1});
    check(!result,"Unsupported modern API cannot select a package");
    std::cout<<checks<<" updater selection checks passed\n";return 0;
 } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
