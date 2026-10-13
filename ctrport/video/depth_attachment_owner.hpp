// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <cstring>
#include <cmath>
#include "graphics_draw_submit.hpp"
#include "sampled_image_upload.hpp"
// Undefined-content depth backing only. No guest tiling or execution admission.
class CtrDepthAttachmentOwner {
    VkImageLayout layout_=VK_IMAGE_LAYOUT_UNDEFINED;VkImageAspectFlags aspects_{};bool failed_=false;
    VkDevice device_{};VkImage image_{};VkDeviceMemory memory_{};VkImageView view_{},sampledView_{};
    PFN_vkDestroyImageView destroyView_{};PFN_vkDestroyImage destroy_{};PFN_vkFreeMemory free_{};
public:
    bool resident=false;
    bool Ready() const{return !failed_ && layout_==VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;}
    VkImageView SampledView() const{return sampledView_;}
    VkImage Image() const{return image_;} VkImageView View() const{return view_;}
    void DrawCompleted(int status){if(status && status!=60 && status!=88)failed_=true;}
    VkFormat format=VK_FORMAT_UNDEFINED;VkExtent2D extent{};uint32_t samples=0,z_format=0,stencil_format=0;
    CtrDepthAttachmentOwner()=default;
    CtrDepthAttachmentOwner(const CtrDepthAttachmentOwner&)=delete;
    CtrDepthAttachmentOwner& operator=(const CtrDepthAttachmentOwner&)=delete;
    ~CtrDepthAttachmentOwner(){if(sampledView_)destroyView_(device_,sampledView_,nullptr);if(view_)destroyView_(device_,view_,nullptr);if(image_)destroy_(device_,image_,nullptr);if(memory_)free_(device_,memory_,nullptr);}
    int DrawBounded(PFN_vkGetDeviceProcAddr get,uint32_t family,VkPipeline pipeline,VkPipelineLayout pipelineLayout,
      VkDescriptorSet descriptors,const VkViewport& viewport,const VkRect2D& scissor,const CtrGraphicsDynamicStateV1& state,
      const void* push,uint32_t count,uint32_t instances,uint32_t firstVertex,uint32_t firstInstance,uint64_t* visibility=nullptr){
        if(resident || failed_ || layout_!=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)return 91;
        if(!(((samples==1 || samples==2) && z_format==3 && stencil_format==1) || (samples==1 && z_format==1 && !stencil_format)))return 88;
        if(state.stencil_test && !stencil_format)return 88;
        bool completed=false;const int status=DrawGraphicsSynchronous(device_,get,family,pipeline,pipelineLayout,descriptors,
          VK_NULL_HANDLE,VK_NULL_HANDLE,extent,viewport,scissor,state.blend_constants,state.cull_mode,state.front_face,
          push,count,instances,firstVertex,firstInstance,completed,0,nullptr,nullptr,nullptr,image_,view_,&state,visibility,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_NULL_HANDLE,VK_NULL_HANDLE,aspects_);
        if(status && status!=60 && status!=88)failed_=true;return status;
    }
    // Host linear D16 snapshot only; no guest tiling or dirty-state publication.
    int ReadD16(PFN_vkGetDeviceProcAddr get,uint32_t family,const VkPhysicalDeviceMemoryProperties& memory,
      void* destination,uint64_t bytes,uint64_t pitch) {
        if(!destination)return 60;
        if(samples!=1 || format!=VK_FORMAT_D16_UNORM || aspects_!=VK_IMAGE_ASPECT_DEPTH_BIT)return 88;
        if(!Ready())return 91;
        bool completed=false;
        const int rc=TransferSampledLinear(device_,get,family,memory,image_,{extent.width,extent.height,1},
          layout_,nullptr,destination,bytes,pitch,completed,layout_,2,VK_IMAGE_ASPECT_DEPTH_BIT);
        if(rc && rc!=60 && rc!=88)failed_=true;
        return rc;
    }
    // Full logical-aspect clear only. Completion and layout persist with this owner.
    // No guest HTILE/writeback semantics are provided here.
    int SampledDescriptor(VkDescriptorImageInfo& output) const {
        if(!sampledView_)return 88;
        if(failed_ || layout_!=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)return 91;
        output={VK_NULL_HANDLE,sampledView_,layout_};return 0;
    }
    // Both aspects transition together; separate depth/stencil layouts are not enabled.
    int SetSampled(PFN_vkGetDeviceProcAddr get,uint32_t family,bool sampled) {
        if(!sampledView_)return 88;
        if(failed_ || layout_==VK_IMAGE_LAYOUT_UNDEFINED)return 91;
        const auto target=sampled?VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        if(layout_==target)return 0;
#define DEPTH_LOAD(name) const auto name=reinterpret_cast<PFN_##name>(get(device_,#name));if(!name)return 88
        DEPTH_LOAD(vkGetDeviceQueue);DEPTH_LOAD(vkCreateCommandPool);DEPTH_LOAD(vkDestroyCommandPool);
        DEPTH_LOAD(vkAllocateCommandBuffers);DEPTH_LOAD(vkBeginCommandBuffer);DEPTH_LOAD(vkEndCommandBuffer);
        DEPTH_LOAD(vkCmdPipelineBarrier);
        DEPTH_LOAD(vkCreateFence);DEPTH_LOAD(vkDestroyFence);DEPTH_LOAD(vkQueueSubmit);DEPTH_LOAD(vkWaitForFences);DEPTH_LOAD(vkQueueWaitIdle);
#undef DEPTH_LOAD
        VkQueue queue{};vkGetDeviceQueue(device_,family,0,&queue);if(!queue)return 91;
        VkCommandPool pool{};VkFence fence{};bool submitted=false,completed=false;
        const auto run=[&]() -> int {
            VkCommandPoolCreateInfo pi{};pi.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;pi.queueFamilyIndex=family;
            if(vkCreateCommandPool(device_,&pi,nullptr,&pool)!=VK_SUCCESS)return 90;
            VkCommandBufferAllocateInfo ai{};ai.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            ai.commandPool=pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
            VkCommandBuffer command{};if(vkAllocateCommandBuffers(device_,&ai,&command)!=VK_SUCCESS)return 90;
            VkCommandBufferBeginInfo bi{};bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if(vkBeginCommandBuffer(command,&bi)!=VK_SUCCESS)return 90;
            VkImageMemoryBarrier barrier{};barrier.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout=layout_;barrier.newLayout=target;
            barrier.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT;
            barrier.dstAccessMask=sampled?VK_ACCESS_SHADER_READ_BIT:VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            barrier.image=image_;barrier.subresourceRange={aspects_,0,1,0,1};
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&barrier);
            if(vkEndCommandBuffer(command)!=VK_SUCCESS)return 90;
            VkFenceCreateInfo fi{};fi.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;if(vkCreateFence(device_,&fi,nullptr,&fence)!=VK_SUCCESS)return 90;
            VkSubmitInfo si{};si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;si.commandBufferCount=1;si.pCommandBuffers=&command;
            if(vkQueueSubmit(queue,1,&si,fence)!=VK_SUCCESS)return 90;
            submitted=true;const auto result=vkWaitForFences(device_,1,&fence,VK_TRUE,5000000000ULL);
            if(result==VK_SUCCESS){completed=true;layout_=target;return 0;}
            return result==VK_TIMEOUT?52:91;
        };
        const int status=run();
        if(submitted && !completed)vkQueueWaitIdle(queue);
        if(status && submitted)failed_=true;
        if(fence)vkDestroyFence(device_,fence,nullptr);
        if(pool)vkDestroyCommandPool(device_,pool,nullptr);
        return status;
    }
    int Clear(PFN_vkGetDeviceProcAddr get,uint32_t family,uint32_t depthBits,uint32_t stencil) {
        if(resident)return 91;
        float depth{};std::memcpy(&depth,&depthBits,sizeof(depth));
        if(!std::isfinite(depth) || depth<0.0f || depth>1.0f || stencil>255)return 60;
        if(failed_)return 91;
        // Separate depth/stencil layouts are not enabled by this device path.
        if(aspects_==VK_IMAGE_ASPECT_STENCIL_BIT)return 88;
#define DEPTH_LOAD(name) const auto name=reinterpret_cast<PFN_##name>(get(device_,#name));if(!name)return 88
        DEPTH_LOAD(vkGetDeviceQueue);DEPTH_LOAD(vkCreateCommandPool);DEPTH_LOAD(vkDestroyCommandPool);
        DEPTH_LOAD(vkAllocateCommandBuffers);DEPTH_LOAD(vkBeginCommandBuffer);DEPTH_LOAD(vkEndCommandBuffer);
        DEPTH_LOAD(vkCmdPipelineBarrier);DEPTH_LOAD(vkCmdClearDepthStencilImage);
        DEPTH_LOAD(vkCreateFence);DEPTH_LOAD(vkDestroyFence);DEPTH_LOAD(vkQueueSubmit);DEPTH_LOAD(vkWaitForFences);DEPTH_LOAD(vkQueueWaitIdle);
#undef DEPTH_LOAD
        VkQueue queue{};vkGetDeviceQueue(device_,family,0,&queue);if(!queue)return 91;
        VkCommandPool pool{};VkFence fence{};bool submitted=false,completed=false;
        const auto run=[&]() -> int {
            VkCommandPoolCreateInfo pi{};pi.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;pi.queueFamilyIndex=family;
            if(vkCreateCommandPool(device_,&pi,nullptr,&pool)!=VK_SUCCESS)return 90;
            VkCommandBufferAllocateInfo ai{};ai.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            ai.commandPool=pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
            VkCommandBuffer command{};if(vkAllocateCommandBuffers(device_,&ai,&command)!=VK_SUCCESS)return 90;
            VkCommandBufferBeginInfo bi{};bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if(vkBeginCommandBuffer(command,&bi)!=VK_SUCCESS)return 90;
            VkImageMemoryBarrier barrier{};barrier.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout=layout_;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcAccessMask=layout_==VK_IMAGE_LAYOUT_UNDEFINED?0:VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            barrier.image=image_;barrier.subresourceRange={aspects_,0,1,0,1};
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
            VkClearDepthStencilValue clear{depth,stencil};
            vkCmdClearDepthStencilImage(command,image_,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&clear,1,&barrier.subresourceRange);
            barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,0,0,nullptr,0,nullptr,1,&barrier);
            if(vkEndCommandBuffer(command)!=VK_SUCCESS)return 90;
            VkFenceCreateInfo fi{};fi.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;if(vkCreateFence(device_,&fi,nullptr,&fence)!=VK_SUCCESS)return 90;
            VkSubmitInfo si{};si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;si.commandBufferCount=1;si.pCommandBuffers=&command;
            if(vkQueueSubmit(queue,1,&si,fence)!=VK_SUCCESS)return 90;
            submitted=true;const auto result=vkWaitForFences(device_,1,&fence,VK_TRUE,5000000000ULL);
            if(result==VK_SUCCESS){completed=true;layout_=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;return 0;}
            return result==VK_TIMEOUT?52:91;
        };
        const int status=run();
        if(submitted && !completed)vkQueueWaitIdle(queue);
        if(status && submitted)failed_=true;
        if(fence)vkDestroyFence(device_,fence,nullptr);
        if(pool)vkDestroyCommandPool(device_,pool,nullptr);
        return status;
    }
    static int Create(VkDevice device,PFN_vkGetDeviceProcAddr get,
        const VkPhysicalDeviceMemoryProperties& properties,const VkImageCreateInfo& info,VkImageAspectFlags aspects,
        std::unique_ptr<CtrDepthAttachmentOwner>& output) {
        if(!device || !get)return 60;
        const auto createView=reinterpret_cast<PFN_vkCreateImageView>(get(device,"vkCreateImageView"));
        const auto destroyView=reinterpret_cast<PFN_vkDestroyImageView>(get(device,"vkDestroyImageView"));
        const auto create=reinterpret_cast<PFN_vkCreateImage>(get(device,"vkCreateImage"));
        const auto requirements=reinterpret_cast<PFN_vkGetImageMemoryRequirements>(get(device,"vkGetImageMemoryRequirements"));
        const auto allocate=reinterpret_cast<PFN_vkAllocateMemory>(get(device,"vkAllocateMemory"));
        const auto bind=reinterpret_cast<PFN_vkBindImageMemory>(get(device,"vkBindImageMemory"));
        const auto destroy=reinterpret_cast<PFN_vkDestroyImage>(get(device,"vkDestroyImage"));
        const auto free=reinterpret_cast<PFN_vkFreeMemory>(get(device,"vkFreeMemory"));
        if(!createView || !destroyView || !create || !requirements || !allocate || !bind || !destroy || !free)return 88;
        auto value=std::make_unique<CtrDepthAttachmentOwner>();
        value->format=info.format;value->extent={info.extent.width,info.extent.height};value->samples=static_cast<uint32_t>(info.samples);
        value->aspects_=aspects;value->destroyView_=destroyView;value->device_=device;value->destroy_=destroy;value->free_=free;
        if(create(device,&info,nullptr,&value->image_)!=VK_SUCCESS)return 90;
        VkMemoryRequirements needed{};requirements(device,value->image_,&needed);
        uint32_t type=0;
        for(;type<properties.memoryTypeCount;++type)
            if((needed.memoryTypeBits&(1U<<type)) &&
                (properties.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))break;
        if(type==properties.memoryTypeCount)return 88;
        VkMemoryAllocateInfo allocation{};allocation.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize=needed.size;allocation.memoryTypeIndex=type;
        if(allocate(device,&allocation,nullptr,&value->memory_)!=VK_SUCCESS ||
            bind(device,value->image_,value->memory_,0)!=VK_SUCCESS)return 90;
        VkImageViewCreateInfo vi{};vi.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image=value->image_;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=info.format;
        vi.subresourceRange={aspects,0,1,0,1};
        if(createView(device,&vi,nullptr,&value->view_)!=VK_SUCCESS)return 90;
        if(info.usage&VK_IMAGE_USAGE_SAMPLED_BIT){
            if(!(aspects&VK_IMAGE_ASPECT_DEPTH_BIT))return 88;
            vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
            // Exact reached tall D16 is consumed as retail R16 swizzle4444.
            // Preserve identity component semantics for every existing depth domain.
            if(info.format==VK_FORMAT_D16_UNORM && info.extent.width==2048 && info.extent.height==8192)
                vi.components={VK_COMPONENT_SWIZZLE_R,VK_COMPONENT_SWIZZLE_R,VK_COMPONENT_SWIZZLE_R,VK_COMPONENT_SWIZZLE_R};
            if(createView(device,&vi,nullptr,&value->sampledView_)!=VK_SUCCESS)return 90;
        }
        output=std::move(value);return 0;
    }
};
