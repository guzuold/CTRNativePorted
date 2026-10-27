// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "guest_copy_host.hpp"
namespace ctrport {
// Alias shared loader ownership, never a no-op raw-pointer owner. Caller revokes
// before changing mapping permissions; metadata spans exclude reserved holes.
inline std::unique_ptr<GuestCopyRegion> make_image_copy_region(
 const std::shared_ptr<std::vector<MappedImage>>& images,std::size_t index,const ImageMetadata& metadata){
 if(!images || index>=images->size() || !(*images)[index] || metadata.spans.empty())throw std::runtime_error("Guest copy image ownership missing");
 std::uint64_t extent=0;for(const auto& span:metadata.spans)extent=(std::max)(extent,span.second);
 auto owner=std::shared_ptr<void>(images,(*images)[index].get());
 return std::make_unique<GuestCopyRegion>(std::move(owner),static_cast<std::size_t>(extent),metadata.spans);
}
}
