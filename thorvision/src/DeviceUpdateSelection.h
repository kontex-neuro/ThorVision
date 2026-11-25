#pragma once

#include <algorithm>
#include <variant>
#include "xdaqvc/update/update.h"

namespace thorvision::updates {
struct ChooseDeviceType {};
using Selection = std::variant<xvc::update::UpdatePlan, xvc::update::DeviceUpdatePlan, ChooseDeviceType>;

// An absent process setting preserves production selection; a present empty/unknown
// value is refused rather than silently falling back to another channel.
inline xvc::update::Result<std::string> update_channel(std::optional<std::string_view> setting)
{
    if (!setting) return std::string{"release"};
    if (*setting == "release" || *setting == "dev") return std::string{*setting};
    return std::unexpected(xvc::update::Error{xvc::update::ErrorCode::VersionNotFound,
        "Invalid THORVISION_DEVICE_UPDATE_CHANNEL: expected release or dev"});
}

inline bool api_compatible(const xvc::update::DeviceDescriptor &device,
                           const xvc::update::VersionSeries &required)
{
    return device.api_version && device.api_version->major() == required.major
        && device.api_version->minor() == required.minor;
}

inline xvc::update::Result<Selection> select(
    const xvc::update::VersionMatrix &matrix, const xvc::update::DeviceDescriptor &device,
    std::string_view updater, std::string_view environment,
    const xvc::update::VersionSeries &required,
    xvc::update::LegacyDeviceProduct product = xvc::update::LegacyDeviceProduct::Unspecified,
    bool manual_check = false, std::optional<xvc::Version> exact_version = std::nullopt)
{
    using namespace xvc::update;
    if (!device.modern && product == LegacyDeviceProduct::Unspecified) {
        const bool has_product_releases = std::ranges::any_of(matrix.device_versions, [&](const VersionEntry &entry) {
            return entry.environment == environment && entry.status == "stable" &&
                   (entry.product_id == "aio" || entry.product_id == "nvr");
        });
        if (api_compatible(device, required) && (manual_check || has_product_releases)) {
            return Selection{ChooseDeviceType{}};
        }
        if (exact_version)
            return std::unexpected(Error{ErrorCode::VersionNotFound,
                "An exact version requires a compatible, identified device"});
        if (environment != "release")
            return std::unexpected(Error{ErrorCode::VersionNotFound,
                "No compatible device migration is available on the development channel"});
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
    auto plan = exact_version
        ? plan_device_version(matrix, device, updater, environment, product, *exact_version)
        : plan_device_update(matrix, device, updater, environment, product);
    if (!plan) return std::unexpected(plan.error());
    return Selection{*plan};
}
} // namespace thorvision::updates
