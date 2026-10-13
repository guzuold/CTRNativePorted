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
inline int guest_copy_image_control(const char* path){
 auto images=std::make_shared<std::vector<MappedImage>>();ImageMetadata metadata;
 images->push_back(load_retail_image(path,false,&metadata));
 if(metadata.executable_spans.empty())return 1;
 const auto address=reinterpret_cast<std::uintptr_t>(images->front().get())+metadata.executable_spans.front().first;
 auto region=make_image_copy_region(images,0,metadata);auto api=make_guest_copy_api(*region);
 unsigned char bytes[64]{};if(api.read(api.context,address,bytes,64))return 1;
 const auto expected=std::vector<unsigned char>(bytes,bytes+64);
 std::weak_ptr<std::vector<MappedImage>> weak=images;images.reset();if(weak.expired())return 1;
 std::memset(bytes,0,64);if(api.read(api.context,address,bytes,64) || std::memcmp(bytes,expected.data(),64))return 1;
 region->revoke();if(!api.read(api.context,address,bytes,64))return 1;
 region.reset();if(!weak.expired())return 1;
 std::cout<<"{\"scope\":\"retail_image_guest_copy_ownership\",\"image_sha256\":"<<std::quoted(metadata.sha256)<<",\"read_bytes\":64,\"owner_retained\":true,\"revoked_rejected\":true,\"owner_released\":true,\"guest_execution\":false}\n";return 0;
}
}
