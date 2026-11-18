#pragma once

#include <variant>
#include "xdaqvc/update/update.h"

namespace thorvision::updates {
using Selection = std::variant<xvc::update::UpdatePlan, xvc::update::DeviceUpdatePlan>;

inline bool api_compatible(const xvc::update::DeviceDescriptor &device,
                           const xvc::update::VersionSeries &required)
{
    return device.api_version && device.api_version->major() == required.major
        && device.api_version->minor() == required.minor;
}

inline xvc::update::Result<Selection> select(
    const xvc::update::VersionMatrix &matrix, const xvc::update::DeviceDescriptor &device,
    std::string_view updater, std::string_view environment,
    const xvc::update::VersionSeries &required, bool legacy_aio_opt_in = false)
{
    using namespace xvc::update;
    if (!device.modern && !legacy_aio_opt_in) {
        if (environment != "release")
            return std::unexpected(Error{ErrorCode::VersionNotFound,
                "Legacy DEV migration requires the explicit AIO maintenance opt-in"});
        if (!device.build_version || !device.api_version)
            return std::unexpected(Error{ErrorCode::VersionNotFound,
                "Legacy application/API version is unknown; update target cannot be guessed"});
        auto plan = plan_update(*device.build_version, matrix, required);
        if (!plan) return std::unexpected(plan.error());
        if (!api_compatible(device, required)) {
            plan->action = UpdateAction::Update;
            plan->required = true;
            plan->reason = "The device API is incompatible; a compatible release is required";
        }
        return Selection{*plan};
    }
    auto plan = plan_device_update(matrix, device, updater, environment, legacy_aio_opt_in);
    if (!plan) return std::unexpected(plan.error());
    return Selection{*plan};
}
} // namespace thorvision::updates
