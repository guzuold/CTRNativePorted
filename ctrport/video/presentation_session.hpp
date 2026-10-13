// SPDX-License-Identifier: GPL-2.0-only
#include "depth_single_extent.hpp"
#pragma once
#include "depth_d16_extent.hpp"
#include "depth_ms2_extent.hpp"
#include "depth_ms2_linear_merge.hpp"
#include <cstdio>
#include "video_display_events.h"
#include "graphics_companion.h"
#include "sampler_preflight.hpp"
#include "sampled_image_owner.hpp"
#include "owned_buffer.hpp"
#include "depth_attachment_preflight.hpp"
#include "depth_attachment_owner.hpp"
#include "color_attachment_owner.hpp"
#include "color_format_preflight.hpp"
#include "graphics_layout_owner.hpp"
#include "graphics_pipeline_owner.hpp"
#include "compute_pipeline_owner.hpp"
#include "graphics_pipeline_state.hpp"
#include "ms_copy_pipeline.hpp"
#include "tiling_pipeline_control.hpp"
#include "output_mode.h"
#include "pm4_init_session.hpp"
#include "pm4_queue_barrier.hpp"
#include "tiled_surface_registry.hpp"
#include <array>
#include <cmath>
#include <climits>
#include <memory>
#include <vector>
#include <deque>
#include "videoout_port.h"
#include "videoout_buffer_input.h"
// Owner-thread synchronous presentation session. Parent keeps loader/instance/surface/window
// live until this owner is destroyed. No guest service, worker, or event queue is installed.
class PresentationSession {
    VkDevice device_; PFN_vkGetDeviceProcAddr get_; VkSwapchainKHR swap_;
    PFN_vkDestroyDevice destroyDevice_; PFN_vkDestroySwapchainKHR destroySwap_;
    uint32_t family_, count_; VkExtent2D extent_; VkFormat format_;
    VkPhysicalDeviceMemoryProperties memory_;
    VkColorSpaceKHR colorSpace_;
    CtrGraphicsProfileV1 shaderProfile_{};
    int shaderProfileStatus_=88;
    VkPhysicalDeviceLimits samplerLimits_{};
    bool samplerAuthority_=false;
    uint32_t graphicsDynamicAuthority_=0,graphicsCoreAuthority_=0,mixedDepthAuthority_=0;
    bool nullDescriptorAuthority_=false,dynamicRenderingAuthority_=false;
    VkPhysicalDevice imagePhysical_{};
    PFN_vkGetPhysicalDeviceFormatProperties imageFormatQuery_{};
    PFN_vkGetPhysicalDeviceImageFormatProperties imageLimitsQuery_{};
    uint64_t layoutToken_=0;
    std::vector<std::pair<uint64_t,std::shared_ptr<CtrGraphicsLayoutOwner>>> graphicsLayouts_;
    uint64_t pipelineToken_=0;
    std::vector<std::pair<uint64_t,std::unique_ptr<CtrGraphicsPipelineOwner>>> graphicsPipelines_;
    uint64_t computePipelineToken_=0;
    std::vector<std::pair<uint64_t,std::unique_ptr<CtrComputePipelineOwner>>> computePipelines_;
    uint64_t colorToken_=0;
    std::vector<std::pair<uint64_t,std::shared_ptr<CtrColorAttachmentOwner>>> colorAttachments_;
    uint64_t depthToken_=0;
    std::vector<std::pair<uint64_t,std::shared_ptr<CtrDepthAttachmentOwner>>> depthAttachments_;
    struct DepthResident {uint64_t generation;CtrDepthBackingSpansV1 spans;std::shared_ptr<CtrDepthAttachmentOwner> owner;};
    uint64_t depthGeneration_=0;
    std::vector<DepthResident> depthResidents_;
    uint64_t bufferToken_=0;
    std::vector<std::pair<uint64_t,std::shared_ptr<CtrOwnedBuffer>>> buffers_;
    uint64_t imageToken_=0;
    std::string tilingDirectory_;
    bool tilingScalar_=false,tilingCompute_=false,tiling16_=false;
    std::vector<std::pair<uint64_t,std::shared_ptr<CtrSampledImageOwner>>> sampledImages_;
    uint64_t samplerToken_=0;
    std::vector<std::pair<uint64_t,std::shared_ptr<CtrOwnedSampler>>> samplers_;

public:
    struct HostFrame { std::vector<unsigned char> bytes; size_t pitch; uint64_t token; uint32_t slot; std::shared_ptr<const unsigned char> owner; };
    using Frame = std::shared_ptr<const HostFrame>;
private:
    std::array<Frame, 16> slots_{};
    struct Group { bool occupied=false; uint32_t first=0,count=0; bool linear=false; CtrVideo::DisplayBufferAttributes attributes{}; };
    std::array<Group,4> groups_{};
    uint64_t nextToken_ = 0;
    const std::shared_ptr<const unsigned char> identity_ = std::make_shared<const unsigned char>(static_cast<unsigned char>(0));
    struct Pending { Frame frame; uint64_t argument; };
    std::deque<Pending> pending_;
    uint64_t completed_ = 0, lastToken_=0, lastArgument_=0;
    int64_t lastSlot_=-1;
    uint64_t submitTsc_=0, completedMicros_=0, completedTsc_=0;
    bool submitClockValid_=false, completeClockValid_=false;
    VideoDisplayEventSet events_;
    uint64_t combinedQueue_=0;
    bool eventsAttached_ = false;
    int eventError_ = 0;
    CtrVideo::VideoOutputPort port_;
    uint64_t portToken_ = 0;
    int32_t flipRate_ = 0;
    CtrPm4::Registers pm4Registers_;
    uint32_t pm4LoadControl_=0,pm4ShadowControl_=0;
    bool pm4Failed_=false;
    uint64_t pm4Completed_=0;
    std::vector<CtrPm4::InitOperation> pm4PendingPlan_;
    std::vector<uint32_t> pm4PendingInput_;
    std::vector<std::shared_ptr<ctrport::DirectMappingLease>> pm4PendingLeases_;
    std::shared_ptr<ctrport::DirectMemoryBacking> pm4PendingOwner_;
    size_t pm4PendingCursor_=0;
    std::shared_ptr<ctrport::VideoLabelMemory> pm4PendingLabels_;
    std::unique_ptr<CtrTiledSurfaceRegistry> tiled_;
    uint64_t endedFrame_=0;uint64_t endedCompletion_=0;uint64_t tiledCompletion_=0;int32_t tiledPreviousSlot_=-1;
    float presentationGamma_=1.0f;
    bool splashHidden_=false;
    HWND window_; bool* closeRequested_;
public:
    PresentationSession(VkDevice device, PFN_vkGetDeviceProcAddr get, VkSwapchainKHR swap,
        PFN_vkDestroyDevice destroyDevice, PFN_vkDestroySwapchainKHR destroySwap,
        uint32_t family, uint32_t count, VkExtent2D extent, VkFormat format, VkColorSpaceKHR colorSpace,
        const VkPhysicalDeviceMemoryProperties& memory, HWND window, bool* closeRequested)
        : device_(device), get_(get), swap_(swap), destroyDevice_(destroyDevice),
          destroySwap_(destroySwap), family_(family), count_(count), extent_(extent),
          format_(format), memory_(memory), colorSpace_(colorSpace), window_(window), closeRequested_(closeRequested) {}
    int QueueMemoryVisibility() {
        if(!window_ || GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(!device_ || !get_)return 91;
        return ctr_pm4_queue_barrier(device_,get_,family_);
    }
    // Called only by the actual device creation path after feature enablement.
    int PreflightVertexInputs(const CtrGraphicsVertexInputsV1* inputs,const uint32_t* formats,uint32_t count) const {
        if(!inputs || inputs->version!=1 || inputs->size!=sizeof(*inputs) || count>32 ||
           count!=inputs->count || (count && !formats))return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(!samplerAuthority_ || !imagePhysical_ || !imageFormatQuery_)return 88;
        if(count>samplerLimits_.maxVertexInputAttributes || count>samplerLimits_.maxVertexInputBindings)return 88;
        uint32_t locations=0,bindings=0;
        for(uint32_t i=0;i<count;++i){const auto& v=inputs->inputs[i];
            if(v.location>=32 || v.binding>=32 || (locations&(1u<<v.location)) || (bindings&(1u<<v.binding)) ||
               v.input_rate>1 || !formats[i] || formats[i]>184)return 60;
            locations|=1u<<v.location;bindings|=1u<<v.binding;
            if(v.location>=samplerLimits_.maxVertexInputAttributes || v.binding>=samplerLimits_.maxVertexInputBindings ||
               v.offset>samplerLimits_.maxVertexInputAttributeOffset || v.stride>samplerLimits_.maxVertexInputBindingStride)return 88;
            // EXT_vertex_attribute_divisor has not been enabled on this session.
            if(v.input_rate && v.divisor!=1)return 88;
            VkFormatProperties properties{};imageFormatQuery_(imagePhysical_,static_cast<VkFormat>(formats[i]),&properties);
            if(!(properties.bufferFeatures&VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT))return 88;
        }
        return 0;
    }
    int PreflightGraphicsDynamic(const CtrGraphicsViewportStateV1* viewport,const CtrGraphicsDynamicStateV1* state,bool depthOnly=false) const {
        if(!viewport || !state || viewport->version!=1 || viewport->size!=sizeof(*viewport) ||
           state->version!=1 || state->size!=sizeof(*state) || !viewport->count || viewport->count>16 ||
           state->depth_test>1 || state->depth_write>1 || state->stencil_test>1 || state->depth_bias>1 ||
           state->depth_bounds_test>1 || state->primitive_restart>1 || state->rasterizer_discard>1 ||
           state->cull_mode>3 || state->front_face>1)return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(!samplerAuthority_ || graphicsDynamicAuthority_!=3 || !dynamicRenderingAuthority_)return 88;
        // MultiViewport, wideLines, list/patch restart and depth-range-unrestricted not enabled.
        if(viewport->count!=1 || (!depthOnly && (state->depth_test || state->stencil_test)) || state->depth_bias ||
           state->depth_bounds_test || state->primitive_restart)return 88;
        if(!std::isfinite(state->line_width))return 60;
        if(state->line_width!=1.0f)return 88;
        for(float constant:state->blend_constants)if(!std::isfinite(constant))return 60;
        const auto& v=viewport->viewports[0];const auto& sc=viewport->scissors[0];
        const float components[]={v.x,v.y,v.width,v.height,v.minDepth,v.maxDepth};
        for(float component:components)if(!std::isfinite(component))return 60;
        const double x0=v.x,x1=double(v.x)+v.width,y0=v.y,y1=double(v.y)+v.height;
        if(v.width<=0 || v.height==0 || v.width>samplerLimits_.maxViewportDimensions[0] ||
           std::abs(double(v.height))>samplerLimits_.maxViewportDimensions[1] ||
           x0<samplerLimits_.viewportBoundsRange[0] || x1>samplerLimits_.viewportBoundsRange[1] ||
           std::min(y0,y1)<samplerLimits_.viewportBoundsRange[0] ||
           std::max(y0,y1)>samplerLimits_.viewportBoundsRange[1] || v.minDepth<0 || v.minDepth>1 ||
           v.maxDepth<0 || v.maxDepth>1 || sc.offset.x<0 || sc.offset.y<0 ||
           uint64_t(sc.offset.x)+sc.extent.width>INT_MAX || uint64_t(sc.offset.y)+sc.extent.height>INT_MAX)return 88;
        return 0;
    }
    int DrawGraphics(uint64_t pipelineToken,uint64_t colorToken,const CtrGraphicsViewportStateV1* viewport,
      const CtrGraphicsDynamicStateV1* dynamic,const CtrGraphicsPushDataV1* push,const CtrGraphicsDrawV1* draw,bool noAttachments=false,uint64_t* visibility=nullptr,uint64_t indexToken=0,uint32_t indexBytes=2,uint64_t indexOffset=0,uint32_t firstIndex=0,int32_t vertexOffset=0){
      if(!push || push->version!=1 || push->size!=sizeof(*push) || !draw || draw->version!=1 || draw->size!=sizeof(*draw) || draw->vertex_buffer_count>32)return 60;
      const int preflight=PreflightGraphicsDynamic(viewport,dynamic);if(preflight)return preflight;
      if(!(graphicsCoreAuthority_&64))return 88;
      if(draw->written_buffers || draw->written_images || draw->atomic_resources || dynamic->rasterizer_discard)return 88;
      CtrGraphicsPipelineOwner* pipeline=nullptr;CtrColorAttachmentOwner* color=nullptr;
      for(auto& e:graphicsPipelines_)if(e.first==pipelineToken)pipeline=e.second.get();
      for(auto& e:colorAttachments_)if(e.first==colorToken)color=e.second.get();
      if(!pipeline || (!noAttachments && !color))return 62;
      if(pipeline->noAttachments!=noAttachments)return 60;if(pipeline->failed)return 91;
      if(noAttachments && (dynamic->depth_test || dynamic->depth_write || dynamic->stencil_test))return 88;
      if(pipeline->colorToken!=colorToken || draw->vertex_buffer_count!=pipeline->vertices.count)return 60;
      std::shared_ptr<CtrOwnedBuffer> indexOwner;VkBuffer indexHandle=VK_NULL_HANDLE;uint32_t indexMin=0,indexMax=0;
      if(indexToken){
        if(noAttachments)return 88;
        for(auto& e:buffers_)if(e.first==indexToken)indexOwner=e.second;
        if(!indexOwner)return 62;
        const int rc=indexOwner->IndexBuffer(indexBytes,indexOffset,firstIndex,draw->vertex_count,indexHandle,indexMin,indexMax);if(rc)return rc;
        if(int64_t(indexMin)+vertexOffset<0 || int64_t(indexMax)+vertexOffset>UINT32_MAX)return 88;
      }
      auto layout=pipeline->Layout();const int resourceReady=layout->ReadyGraphics();if(resourceReady)return resourceReady;
      std::array<std::shared_ptr<CtrOwnedBuffer>,32> owners{};std::array<VkBuffer,32> handles{};
      std::array<uint32_t,32> bindings{};std::array<VkDeviceSize,32> strides{};
      for(uint32_t i=0;i<draw->vertex_buffer_count;++i){
        const auto& input=pipeline->vertices.inputs[i];
        for(auto& e:buffers_)if(e.first==draw->vertex_buffers[i])owners[i]=e.second;
        if(!owners[i])return 62;const int status=owners[i]->VertexBuffer(input.buffer_size,handles[i]);if(status){std::fprintf(stderr,"MRT vertex admission slot=%u status=%d\n",i,status);return status;}
        uint32_t element=0;switch(pipeline->formats[i]){case 37:case 83:element=4;break;case 103:element=8;break;case 106:element=12;break;case 109:element=16;break;default:return 88;}
        const uint64_t elements=input.input_rate?draw->instance_count:indexToken?1:draw->vertex_count;
        const uint64_t first=input.input_rate?draw->first_instance:indexToken?uint64_t(int64_t(indexMax)+vertexOffset):draw->first_vertex;
        if(elements && uint64_t(input.offset)+(first+elements-1)*input.stride+element>input.buffer_size)return 88;
        bindings[i]=input.binding;strides[i]=input.stride;
      }
      const auto& v=viewport->viewports[0];const auto& sc=viewport->scissors[0];
      VkViewport vp{v.x,v.y,v.width,v.height,v.minDepth,v.maxDepth};VkRect2D rect{{sc.offset.x,sc.offset.y},{sc.extent.width,sc.extent.height}};
      if(noAttachments){
        const uint64_t width=uint64_t(sc.offset.x)+sc.extent.width,height=uint64_t(sc.offset.y)+sc.extent.height;
        if(!samplerAuthority_ || !(samplerLimits_.framebufferNoAttachmentsSampleCounts&VK_SAMPLE_COUNT_1_BIT) ||
           !width || !height || width>samplerLimits_.maxFramebufferWidth || height>samplerLimits_.maxFramebufferHeight)return 88;
        bool completed=false;const int rc=DrawGraphicsSynchronous(device_,get_,family_,pipeline->Pipeline(),layout->PipelineLayout(),layout->DescriptorSet(),
          VK_NULL_HANDLE,VK_NULL_HANDLE,{static_cast<uint32_t>(width),static_cast<uint32_t>(height)},vp,rect,dynamic->blend_constants,dynamic->cull_mode,dynamic->front_face,
          push->bytes,draw->vertex_count,draw->instance_count,draw->first_vertex,draw->first_instance,completed,draw->vertex_buffer_count,bindings.data(),handles.data(),strides.data(),
          VK_NULL_HANDLE,VK_NULL_HANDLE,nullptr,visibility,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_NULL_HANDLE,VK_NULL_HANDLE,0,true);
        if(rc && rc!=60 && rc!=88)pipeline->failed=true;return rc;
      }
      return color->DrawBounded(get_,family_,pipeline->Pipeline(),layout->PipelineLayout(),layout->DescriptorSet(),vp,rect,
        dynamic->blend_constants,dynamic->cull_mode,dynamic->front_face,push->bytes,draw->vertex_count,draw->instance_count,
        draw->first_vertex,draw->first_instance,draw->vertex_buffer_count,bindings.data(),handles.data(),strides.data(),indexHandle,indexBytes==2?VK_INDEX_TYPE_UINT16:VK_INDEX_TYPE_UINT32,indexOffset,firstIndex,vertexOffset);
    }
    int PreflightIndexedColorPipeline(uint64_t token) const {
      if(!window_ || !IsWindow(window_))return 91;
      if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
      for(const auto& e:graphicsPipelines_)if(e.first==token){const auto& p=*e.second;
       return p.noAttachments || p.mrtDepthOwner || p.mrtOwners[0] || p.mrtOwners[1]?88:0;
      }return 62;
    }
    int PreflightIndexedDepthPipeline(uint64_t token) const {
      if(!window_ || !IsWindow(window_))return 91;
      if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
      for(const auto& e:graphicsPipelines_)if(e.first==token){const auto& p=*e.second;
       if(!p.mrtOwners[0] || p.mrtOwners[1] || !p.mrtDepthOwner)return 88;
       const auto& depth=*p.mrtDepthOwner;
       if(depth.samples!=2 || depth.z_format!=3 || depth.stencil_format!=1)return 88;
       return 0;
      }return 62;
    }
    int DrawMrtDepth(uint64_t pipelineToken,const CtrGraphicsViewportStateV1* viewport,
      const CtrGraphicsDynamicStateV1* dynamic,const CtrGraphicsPushDataV1* push,const CtrGraphicsDrawV1* draw,uint64_t* visibility=nullptr,bool requireDepth=true,uint64_t indexToken=0,uint32_t indexBytes=2,uint64_t indexOffset=0,uint32_t firstIndex=0,int32_t vertexOffset=0){
      if(!push || push->version!=1 || push->size!=sizeof(*push) || !draw || draw->version!=1 || draw->size!=sizeof(*draw) || draw->vertex_buffer_count>32)return 60;
      const int preflight=PreflightGraphicsDynamic(viewport,dynamic,true);if(preflight)return preflight;
      if(!(graphicsCoreAuthority_&64))return 88;
      if(draw->written_buffers || draw->written_images || draw->atomic_resources || dynamic->rasterizer_discard)return 88;
      CtrGraphicsPipelineOwner* pipeline=nullptr;
      for(auto& e:graphicsPipelines_)if(e.first==pipelineToken)pipeline=e.second.get();
      if(!pipeline)return 62;
      auto depth=pipeline->mrtDepthOwner;if(requireDepth!=bool(depth))return 60; if(!depth && (dynamic->depth_test || dynamic->depth_write || dynamic->stencil_test))return 88; if(depth && (depth->resident || !depth->Ready())){std::fprintf(stderr,"MRT depth admission pipeline=%llu owner=%u ready=false status91\n",static_cast<unsigned long long>(pipelineToken),depth?1u:0u);return 91;}
      CtrColorAttachmentOwner* colors[2]{};
      const uint32_t colorCount=pipeline->mrtOwners[1]?2u:1u;
      for(uint32_t i=0;i<colorCount;++i){if(!pipeline->mrtOwners[i])return 60;colors[i]=(pipeline->colorMs2WithoutDepth || (depth && depth->samples==2))?pipeline->mrtOwners[i]->SampleBacking():pipeline->mrtOwners[i].get();
       if(!colors[i] || colors[i]->Failed() || colors[i]->Layout()!=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL){std::fprintf(stderr,"MRT color admission slot=%u owner=%u failed=%u layout=%d status91\n",i,colors[i]?1u:0u,colors[i]&&colors[i]->Failed()?1u:0u,colors[i]?static_cast<int>(colors[i]->Layout()):-1);return 91;}
      }
      if(draw->vertex_buffer_count!=pipeline->vertices.count)return 60;
      std::shared_ptr<CtrOwnedBuffer> indexOwner;VkBuffer indexHandle=VK_NULL_HANDLE;uint32_t indexMin=0,indexMax=0;
      if(indexToken){

        for(auto& e:buffers_)if(e.first==indexToken)indexOwner=e.second;
        if(!indexOwner)return 62;
        const int rc=indexOwner->IndexBuffer(indexBytes,indexOffset,firstIndex,draw->vertex_count,indexHandle,indexMin,indexMax);if(rc)return rc;
        if(int64_t(indexMin)+vertexOffset<0 || int64_t(indexMax)+vertexOffset>UINT32_MAX)return 88;
      }
      auto layout=pipeline->Layout();const int ready=layout->ReadyGraphics();if(ready){std::fprintf(stderr,"MRT descriptor admission status=%d\n",ready);return ready;}
      std::array<std::shared_ptr<CtrOwnedBuffer>,32> owners{};std::array<VkBuffer,32> handles{};
      std::array<uint32_t,32> bindings{};std::array<VkDeviceSize,32> strides{};
      for(uint32_t i=0;i<draw->vertex_buffer_count;++i){
        const auto& input=pipeline->vertices.inputs[i];
        for(auto& e:buffers_)if(e.first==draw->vertex_buffers[i])owners[i]=e.second;
        if(!owners[i])return 62;const int status=owners[i]->VertexBuffer(input.buffer_size,handles[i]);if(status){std::fprintf(stderr,"MRT vertex admission slot=%u status=%d\n",i,status);return status;}
        uint32_t element=0;switch(pipeline->formats[i]){case 37:case 83:element=4;break;case 103:element=8;break;case 106:element=12;break;case 109:element=16;break;default:return 88;}
        const uint64_t elements=input.input_rate?draw->instance_count:indexToken?1:draw->vertex_count;
        const uint64_t first=input.input_rate?draw->first_instance:indexToken?uint64_t(int64_t(indexMax)+vertexOffset):draw->first_vertex;
        if(elements && uint64_t(input.offset)+(first+elements-1)*input.stride+element>input.buffer_size)return 88;
        bindings[i]=input.binding;strides[i]=input.stride;
      }
      const auto& v=viewport->viewports[0];const auto& sc=viewport->scissors[0];
      VkViewport vp{v.x,v.y,v.width,v.height,v.minDepth,v.maxDepth};VkRect2D rect{{sc.offset.x,sc.offset.y},{sc.extent.width,sc.extent.height}};
      const VkExtent2D extent{std::min({colors[0]->Extent().width,colors[1]?colors[1]->Extent().width:UINT32_MAX,depth?depth->extent.width:UINT32_MAX}),
       std::min({colors[0]->Extent().height,colors[1]?colors[1]->Extent().height:UINT32_MAX,depth?depth->extent.height:UINT32_MAX})};
      if(uint64_t(rect.offset.x)+rect.extent.width>extent.width || uint64_t(rect.offset.y)+rect.extent.height>extent.height)return 88;
      bool completed=false;const int rc=DrawGraphicsSynchronous(device_,get_,family_,pipeline->Pipeline(),layout->PipelineLayout(),layout->DescriptorSet(),
       colors[0]->Image(),colors[0]->View(),extent,vp,rect,dynamic->blend_constants,dynamic->cull_mode,dynamic->front_face,
       push->bytes,draw->vertex_count,draw->instance_count,draw->first_vertex,draw->first_instance,completed,draw->vertex_buffer_count,
       bindings.data(),handles.data(),strides.data(),depth?depth->Image():VK_NULL_HANDLE,depth?depth->View():VK_NULL_HANDLE,depth?dynamic:nullptr,visibility,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
       colors[1]?colors[1]->Image():VK_NULL_HANDLE,colors[1]?colors[1]->View():VK_NULL_HANDLE,VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,false,indexHandle,indexBytes==2?VK_INDEX_TYPE_UINT16:VK_INDEX_TYPE_UINT32,indexOffset,firstIndex,vertexOffset,pipeline->inactiveSecondColor);
      if(rc)std::fprintf(stderr,"MRT Vulkan draw submission status=%d completed=%u\n",rc,completed?1u:0u);
      colors[0]->CopyCompleted(rc);if(colors[1])colors[1]->CopyCompleted(rc);if(depth)depth->DrawCompleted(rc);return rc;
    }
    int DrawRectListDepth(uint64_t pipelineToken,uint64_t depthToken,const CtrGraphicsViewportStateV1* viewport,
      const CtrGraphicsDynamicStateV1* dynamic,const CtrGraphicsPushDataV1* push,const CtrGraphicsDrawV1* draw,uint64_t* visibility=nullptr){
      if(!push || push->version!=1 || push->size!=sizeof(*push) || !draw || draw->version!=1 || draw->size!=sizeof(*draw))return 60;
      const int checked=PreflightGraphicsDynamic(viewport,dynamic,true);if(checked)return checked;
      if(!(graphicsCoreAuthority_&64) || draw->vertex_buffer_count || draw->written_buffers || draw->written_images || draw->atomic_resources || dynamic->rasterizer_discard)return 88;
      CtrGraphicsPipelineOwner* pipeline=nullptr;CtrDepthAttachmentOwner* depth=nullptr;
      for(auto& entry:graphicsPipelines_)if(entry.first==pipelineToken)pipeline=entry.second.get();
      for(auto& entry:depthAttachments_)if(entry.first==depthToken)depth=entry.second.get();
      if(!pipeline || !depth)return 62;if(pipeline->depthToken!=depthToken || pipeline->colorToken)return 60;
      auto layout=pipeline->Layout();if(!layout->ReadyBuffersOnly())return 88;
      const auto& v=viewport->viewports[0];const auto& sc=viewport->scissors[0];
      const VkViewport vp{v.x,v.y,v.width,v.height,v.minDepth,v.maxDepth};const VkRect2D rect{{sc.offset.x,sc.offset.y},{sc.extent.width,sc.extent.height}};
      return depth->DrawBounded(get_,family_,pipeline->Pipeline(),layout->PipelineLayout(),layout->DescriptorSet(),vp,rect,*dynamic,push->bytes,
        draw->vertex_count,draw->instance_count,draw->first_vertex,draw->first_instance,visibility);
    }
    void SetMixedDepthAuthority(uint32_t mask){mixedDepthAuthority_=mask;}
    int MixedDepthAuthority(uint32_t* output) const {
        if(!output)return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        *output=mixedDepthAuthority_;return 0;
    }
    void SetGraphicsCoreAuthority(uint32_t mask){graphicsCoreAuthority_=mask;}
    int GraphicsCoreAuthority(uint32_t required) const {
        if(!required || (required&~8191u))return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        return (graphicsCoreAuthority_&required)==required?0:88;
    }
    void SetGraphicsDynamicAuthority(uint32_t mask){graphicsDynamicAuthority_=mask;}
    int GraphicsDynamicAuthority(uint32_t required) const {
        if(!required || (required&~3u))return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        return (graphicsDynamicAuthority_&required)==required?0:88;
    }
    void SetDynamicRenderingAuthority(bool enabled){dynamicRenderingAuthority_=enabled;}
    int DynamicRenderingAuthority() const {
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        return dynamicRenderingAuthority_?0:88;
    }
    void SetNullDescriptorAuthority(bool enabled){nullDescriptorAuthority_=enabled;}
    int NullDescriptorAuthority() const {return nullDescriptorAuthority_?0:88;}
    void SetSamplerLimits(const VkPhysicalDeviceLimits& limits) {samplerLimits_=limits;samplerAuthority_=true;}
    void SetTilingAuthority(const char* directory,bool scalar,bool compute,bool storage16=false) {
        tilingDirectory_=directory?directory:"";tilingScalar_=scalar;tilingCompute_=compute;tiling16_=storage16;
    }
    void SetImageAuthority(VkPhysicalDevice physical,PFN_vkGetPhysicalDeviceFormatProperties format,
        PFN_vkGetPhysicalDeviceImageFormatProperties limits) {
        imagePhysical_=physical;imageFormatQuery_=format;imageLimitsQuery_=limits;
    }
    int PreflightDepth(const CtrPm4::DepthAttachmentShape& shape,VkImageCreateInfo& info,VkImageAspectFlags& aspects) {
        if(!window_ || !IsWindow(window_) || !imagePhysical_)return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        return CtrPm4::PreflightDepthAttachment(shape,imagePhysical_,imageFormatQuery_,imageLimitsQuery_,info,aspects);
    }
    int CreateGraphicsLayout(const CtrGraphicsBindingContractV1* ps,const CtrGraphicsBindingContractV1* vs,uint64_t* token,bool fragmentPresent=true,bool compute=false) {
        if((fragmentPresent && !ps) || !vs || !token)return 60;
        // Neutral initial binding counters, not a fragment stage or compiled record.
        CtrGraphicsBindingContractV1 origin{};origin.version=1;origin.size=sizeof(origin);
        if(!fragmentPresent)ps=&origin;
        if(!window_ || !IsWindow(window_) || !samplerAuthority_)return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(layoutToken_==UINT64_MAX)return 88;
        graphicsLayouts_.reserve(graphicsLayouts_.size()+1);
        std::unique_ptr<CtrGraphicsLayoutOwner> value;
        const int status=CtrGraphicsLayoutOwner::Create(device_,get_,samplerLimits_,*ps,*vs,value,fragmentPresent,compute);
        if(status)return status;
        const auto id=++layoutToken_;graphicsLayouts_.emplace_back(id,std::move(value));*token=id;return 0;
    }
    int CreateComputePipeline(uint64_t layoutToken,const uint32_t* code,uint32_t words,uint64_t* token){
        if(!token)return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(computePipelineToken_==UINT64_MAX)return 88;
        std::shared_ptr<CtrGraphicsLayoutOwner> layout;const int found=RetainGraphicsLayout(layoutToken,layout);if(found)return found;
        computePipelines_.reserve(computePipelines_.size()+1);std::unique_ptr<CtrComputePipelineOwner> value;
        const int status=CtrComputePipelineOwner::Create(device_,get_,layout,code,words,value);if(status)return status;
        const auto id=++computePipelineToken_;computePipelines_.emplace_back(id,std::move(value));*token=id;return 0;
    }
    int DispatchCompute(uint64_t token,const CtrGraphicsPushDataV1* push,const uint32_t* groups){
        if(!token || !push || push->version!=1 || push->size!=sizeof(*push) || !groups)return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(!samplerAuthority_)return 88;
        for(uint32_t i=0;i<3;++i)if(!groups[i] || groups[i]>samplerLimits_.maxComputeWorkGroupCount[i])return 88;
        for(auto& entry:computePipelines_)if(entry.first==token)return entry.second->Dispatch(get_,family_,push->bytes,groups);return 62;
    }
    int ReleaseComputePipeline(uint64_t token){
        if(!token)return 60;if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto it=computePipelines_.begin();it!=computePipelines_.end();++it)if(it->first==token){computePipelines_.erase(it);return 0;}return 62;
    }
    // Internal owner acquisition for pipelines; never export raw Vulkan handles through C ABI.
    int RetainGraphicsLayout(uint64_t token,std::shared_ptr<CtrGraphicsLayoutOwner>& output) const {
        if(!token)return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(const auto& entry:graphicsLayouts_)if(entry.first==token){output=entry.second;return 0;}
        return 62;
    }
    int CreateGraphicsPipeline(const CtrGraphicsKeyV1* key,const CtrGraphicsVertexInputsV1* vertices,
      const uint32_t* formats,uint32_t formatCount,uint64_t layoutToken,uint64_t colorToken,
      const uint32_t* vs,uint32_t vsWords,const uint32_t* ps,uint32_t psWords,
      uint32_t sampleShading,uint64_t* token,bool noAttachments=false) {
        if(!key || !vertices || !token || formatCount!=vertices->count)return 60;
        const int vertexStatus=PreflightVertexInputs(vertices,formats,formatCount);if(vertexStatus)return vertexStatus;
        if(!dynamicRenderingAuthority_ || graphicsDynamicAuthority_!=3)return 88;
        std::shared_ptr<CtrGraphicsLayoutOwner> layout;
        const int layoutStatus=RetainGraphicsLayout(layoutToken,layout);if(layoutStatus)return layoutStatus;
        CtrColorAttachmentOwner* color=nullptr;
        for(auto& entry:colorAttachments_)if(entry.first==colorToken)color=entry.second.get();
        if(!noAttachments){
        if(!color)return 62;
        if(key->colors[0].data_format!=color->requirement.data_format ||
           key->colors[0].number_format!=color->requirement.number_format || color->requirement.samples!=1)return 88;
        }
        VkFormat checked=VK_FORMAT_UNDEFINED;
        if(!noAttachments){
        const int colorStatus=CtrPm4::PreflightColorFormat(color->requirement,imagePhysical_,imageFormatQuery_,key->colors[0].blend_enable!=0,checked);
        if(colorStatus)return colorStatus;if(checked!=color->Format())return 88;
        }
        if(noAttachments && (!samplerAuthority_ || !(samplerLimits_.framebufferNoAttachmentsSampleCounts&VK_SAMPLE_COUNT_1_BIT)))return 88;
        CtrGraphicsPipelineState state;
        const int status=state.Prepare(*key,*vertices,formats,checked,graphicsCoreAuthority_,sampleShading,VK_FORMAT_UNDEFINED,VK_FORMAT_UNDEFINED,noAttachments);if(status)return status;
        if(pipelineToken_==UINT64_MAX)return 88;
        graphicsPipelines_.reserve(graphicsPipelines_.size()+1);
        std::unique_ptr<CtrGraphicsPipelineOwner> value;
        const int allocated=CtrGraphicsPipelineOwner::Create(device_,get_,layout,state.pipeline,vs,vsWords,ps,psWords,value);
        if(allocated)return allocated;
        value->vertices=*vertices;for(uint32_t i=0;i<formatCount;++i)value->formats[i]=formats[i];value->colorToken=colorToken;value->noAttachments=noAttachments;
        const auto id=++pipelineToken_;graphicsPipelines_.emplace_back(id,std::move(value));*token=id;return 0;
    }
    int CreateMrtPipeline(const CtrGraphicsKeyV1* key,const CtrGraphicsVertexInputsV1* vertices,
      const uint32_t* formats,uint32_t formatCount,uint64_t layoutToken,const uint64_t* colors,uint32_t colorCount,
      const uint32_t* vs,uint32_t vsWords,const uint32_t* ps,uint32_t psWords,
      uint32_t sampleShading,uint64_t* token,uint64_t depthToken=0) {
        if(!key || !vertices || !token || !colors || (colorCount!=1 && colorCount!=2) || (colorCount==2 && colors[0]==colors[1]) || formatCount!=vertices->count)return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        const int vertexStatus=PreflightVertexInputs(vertices,formats,formatCount);if(vertexStatus)return vertexStatus;
        if(!dynamicRenderingAuthority_ || graphicsDynamicAuthority_!=3)return 88;
        std::shared_ptr<CtrGraphicsLayoutOwner> layout;
        const int layoutStatus=RetainGraphicsLayout(layoutToken,layout);if(layoutStatus)return layoutStatus;
        const bool inactiveSecond=colorCount==2 && CtrInactiveSecondColor(*key) && !depthToken;
        if(inactiveSecond && colors[1])return 88;
        std::array<std::shared_ptr<CtrColorAttachmentOwner>,2> retained{};
        VkFormat checked[2]{};
        for(uint32_t i=0;i<colorCount;++i){
         if(i==1 && inactiveSecond)continue;
         for(auto& entry:colorAttachments_)if(entry.first==colors[i])retained[i]=entry.second;
         if(!retained[i])return 62;const auto& color=*retained[i];
         if(key->colors[i].data_format!=color.requirement.data_format || key->colors[i].number_format!=color.requirement.number_format || color.requirement.samples!=1)return 88;
         const int check=CtrPm4::PreflightColorFormat(color.requirement,imagePhysical_,imageFormatQuery_,key->colors[i].blend_enable!=0,checked[i]);
         if(check)return check;if(checked[i]!=color.Format())return 88;
        }
        std::shared_ptr<CtrDepthAttachmentOwner> depth;
        if(depthToken){
         for(auto& entry:depthAttachments_)if(entry.first==depthToken)depth=entry.second;
         if(!depth)return 62;
         if(depth->samples!=key->num_samples || depth->samples!=key->depth_samples || depth->z_format!=key->z_format || depth->stencil_format!=key->stencil_format)return 88;
         if(depth->samples==2)for(uint32_t i=0;i<colorCount;++i){const auto* backing=retained[i]->SampleBacking();
          if(!backing || backing->Failed() || backing->Layout()!=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)return 88;
         }
        }
        const bool inactiveMs2=!depth && key->num_samples==2;
        if(inactiveMs2)for(uint32_t i=0;i<colorCount;++i){const auto* backing=retained[i]->SampleBacking();if(!backing || backing->Failed() || backing->Layout()!=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)return 88;}
        CtrGraphicsPipelineState state;
        const int status=state.Prepare(*key,*vertices,formats,checked[0],graphicsCoreAuthority_,sampleShading,depth?depth->format:VK_FORMAT_UNDEFINED,checked[1],false,inactiveMs2,inactiveSecond);if(status)return status;
        if(pipelineToken_==UINT64_MAX)return 88;
        graphicsPipelines_.reserve(graphicsPipelines_.size()+1);
        std::unique_ptr<CtrGraphicsPipelineOwner> value;
        const int allocated=CtrGraphicsPipelineOwner::Create(device_,get_,layout,state.pipeline,vs,vsWords,ps,psWords,value);
        if(allocated)return allocated;
        value->vertices=*vertices;for(uint32_t i=0;i<formatCount;++i)value->formats[i]=formats[i];value->mrtOwners=std::move(retained);value->mrtTokens={colors[0],colorCount==2?colors[1]:0};value->mrtDepthOwner=depth;value->depthToken=depthToken;value->colorMs2WithoutDepth=inactiveMs2;value->inactiveSecondColor=inactiveSecond;
        const auto id=++pipelineToken_;graphicsPipelines_.emplace_back(id,std::move(value));*token=id;return 0;
    }
    int CreateRectListDepthPipeline(const CtrGraphicsKeyV1* key,uint64_t layoutToken,uint64_t depthToken,
      const uint32_t* vs,uint32_t vsWords,const uint32_t* tcs,uint32_t tcsWords,
      const uint32_t* tes,uint32_t tesWords,uint64_t* token,uint32_t controlSampleMask=UINT32_MAX,bool noAttachments=false){
        if(!key || key->version!=1 || key->size!=sizeof(*key) || !token)return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(!dynamicRenderingAuthority_ || graphicsDynamicAuthority_!=3)return 88;
        std::shared_ptr<CtrGraphicsLayoutOwner> layout;
        const int retained=RetainGraphicsLayout(layoutToken,layout);if(retained)return retained;
        CtrDepthAttachmentOwner* depth=nullptr;
        for(auto& entry:depthAttachments_)if(entry.first==depthToken)depth=entry.second.get();
        if(noAttachments){if(depthToken)return 60;}else{if(!depth)return 62;if(depth->samples!=key->num_samples || depth->samples!=key->depth_samples || depth->z_format!=key->z_format || depth->stencil_format!=key->stencil_format)return 88;}
        CtrGraphicsVertexInputsV1 inputs{};inputs.version=1;inputs.size=sizeof(inputs);
        CtrGraphicsPipelineState state;
        const int prepared=state.Prepare(*key,inputs,nullptr,VK_FORMAT_UNDEFINED,graphicsCoreAuthority_,0,noAttachments?VK_FORMAT_UNDEFINED:depth->format,VK_FORMAT_UNDEFINED,noAttachments);
        if(prepared)return prepared;if(noAttachments && (!samplerAuthority_ || !(samplerLimits_.framebufferNoAttachmentsSampleCounts&VK_SAMPLE_COUNT_1_BIT)))return 88;if(pipelineToken_==UINT64_MAX)return 88;
        graphicsPipelines_.reserve(graphicsPipelines_.size()+1);
        std::unique_ptr<CtrGraphicsPipelineOwner> value;
        // Authored GPU sample-routing control only; public CABI keeps all samples enabled.
        if(controlSampleMask!=UINT32_MAX)state.samples.pSampleMask=&controlSampleMask;
        const int created=CtrGraphicsPipelineOwner::CreateRectListVertexOnly(device_,get_,layout,state.pipeline,vs,vsWords,tcs,tcsWords,tes,tesWords,value);
        if(created)return created;value->vertices=inputs;value->depthToken=depthToken;value->noAttachments=noAttachments;
        const auto id=++pipelineToken_;graphicsPipelines_.emplace_back(id,std::move(value));*token=id;return 0;
    }
    int ReleaseGraphicsPipeline(uint64_t token){
        if(!token)return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto i=graphicsPipelines_.begin();i!=graphicsPipelines_.end();++i)if(i->first==token){graphicsPipelines_.erase(i);return 0;}
        return 62;
    }
    int WriteGraphicsImages(uint64_t token,const uint32_t* bindings,const uint64_t* tokens,uint32_t count,const uint32_t* nulls=nullptr) {
        if(!token || count>240 || (count && (!bindings || !tokens)))return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        CtrGraphicsLayoutOwner* layout=nullptr;
        for(auto& entry:graphicsLayouts_)if(entry.first==token)layout=entry.second.get();
        if(!layout)return 62;
        std::vector<std::shared_ptr<CtrSampledImageOwner>> owners(count);
        for(uint32_t i=0;i<count;++i){
            if(nulls && nulls[i]>1)return 60;
            if(nulls && nulls[i]){if(tokens[i])return 60;continue;}
            for(auto& entry:sampledImages_)if(entry.first==tokens[i])owners[i]=entry.second;
            if(!owners[i])return 62;
        }
        return layout->WriteImages(get_,bindings,count,owners,nulls,nullDescriptorAuthority_);
    }
    int WriteGraphicsSamplers(uint64_t token,const uint32_t* bindings,const uint64_t* tokens,uint32_t count) {
        if(!token || count>240 || (count && (!bindings || !tokens)))return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        CtrGraphicsLayoutOwner* layout=nullptr;
        for(auto& entry:graphicsLayouts_)if(entry.first==token)layout=entry.second.get();
        if(!layout)return 62;
        std::vector<std::shared_ptr<CtrOwnedSampler>> owners(count);
        for(uint32_t i=0;i<count;++i){
            for(auto& entry:samplers_)if(entry.first==tokens[i])owners[i]=entry.second;
            if(!owners[i])return 62;
        }
        return layout->WriteSamplers(get_,bindings,count,owners);
    }
    int WriteGraphicsBuffers(uint64_t token,const CtrGraphicsBufferWriteV1* writes,uint32_t count) {
        if(!token || count>240 || (count && !writes))return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        CtrGraphicsLayoutOwner* layout=nullptr;
        for(auto& entry:graphicsLayouts_)if(entry.first==token)layout=entry.second.get();
        if(!layout)return 62;
        std::vector<std::shared_ptr<CtrOwnedBuffer>> owners(count);
        for(uint32_t i=0;i<count;++i)if(writes[i].buffer_token){
            for(auto& entry:buffers_)if(entry.first==writes[i].buffer_token)owners[i]=entry.second;
            if(!owners[i])return 62;
        }
        return layout->WriteBuffers(get_,nullDescriptorAuthority_,writes,count,owners);
    }
    int ReleaseGraphicsLayout(uint64_t token) {
        if(!token)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto i=graphicsLayouts_.begin();i!=graphicsLayouts_.end();++i)if(i->first==token){graphicsLayouts_.erase(i);return 0;}
        return 62;
    }
    int CreateColorAttachment(const CtrGraphicsColorRequirementV1* p,uint32_t blending,uint64_t* token) {
        if(!p || !token || blending>1)return 60;
        if(!window_ || !IsWindow(window_) || !imagePhysical_)return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        VkImageCreateInfo info{};
        const int status=CtrPm4::PreflightColorImage(*p,imagePhysical_,imageFormatQuery_,imageLimitsQuery_,blending!=0,info,true);
        if(status)return status;
        if(colorToken_==UINT64_MAX)return 88;
        colorAttachments_.reserve(colorAttachments_.size()+1);
        std::unique_ptr<CtrColorAttachmentOwner> image;
        const int created=CtrColorAttachmentOwner::Create(device_,get_,memory_,info,VK_IMAGE_ASPECT_COLOR_BIT,image);
        if(created)return created;
        image->requirement=*p;
        const auto value=++colorToken_;colorAttachments_.emplace_back(value,std::move(image));*token=value;return 0;
    }
    // Allocation only: alternate backing remains UNDEFINED and cannot be drawn/read.
    int AllocateColorSampleBacking(uint64_t token){
        if(!token)return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto& entry:colorAttachments_)if(entry.first==token){
            auto demand=entry.second->requirement;demand.samples=2;
            VkImageCreateInfo info{};const int rc=CtrPm4::PreflightColorImage(demand,imagePhysical_,imageFormatQuery_,imageLimitsQuery_,false,info,true);
            if(rc)return rc;return entry.second->AllocateSampleBacking(get_,memory_,info);
        }return 62;
    }
    int CopyColorBackingPublic(uint64_t token,uint32_t direction){
        if(!token || direction>1)return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(tilingDirectory_.empty())return 88;
        std::vector<uint32_t> vertex,fragment;
        const auto load=[&](const char* name,std::vector<uint32_t>& output)->int{
            std::ifstream input(tilingDirectory_+"/"+name,std::ios::binary|std::ios::ate);if(!input)return 88;
            const auto size=input.tellg();if(size<20 || size>16777216 || (static_cast<uint64_t>(size)&3))return 60;
            output.resize(static_cast<size_t>(size)/4);input.seekg(0);input.read(reinterpret_cast<char*>(output.data()),size);return input?0:60;
        };
        int rc=load("ms_copy_vertex.spv",vertex);if(rc)return rc;
        rc=load(direction?"ms_copy_from_multi.spv":"ms_copy_from_single.spv",fragment);if(rc)return rc;
        if(!direction){rc=AllocateColorSampleBacking(token);if(rc)return rc;}
        return CopyColorBacking(token,direction==0,vertex.data(),static_cast<uint32_t>(vertex.size()),fragment.data(),static_cast<uint32_t>(fragment.size()));
    }
    int CopyColorBacking(uint64_t token,bool toMs,const uint32_t* vs,uint32_t vsWords,const uint32_t* ps,uint32_t psWords){
        if(!token)return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(!dynamicRenderingAuthority_ || graphicsDynamicAuthority_!=3 || !(graphicsCoreAuthority_&64))return 88;
        for(auto& entry:colorAttachments_)if(entry.first==token){
            auto* single=entry.second.get();auto* multi=single->SampleBacking();if(!multi)return 88;
            std::unique_ptr<CtrGraphicsPipelineOwner> pipeline;
            int rc=CreateMsBackingCopyPipeline(device_,get_,samplerLimits_,single->Format(),toMs?2u:1u,vs,vsWords,ps,psWords,graphicsCoreAuthority_,pipeline);
            if(rc)return rc;
            return CopyMsBacking(device_,get_,family_,*pipeline,toMs?*single:*multi,toMs?*multi:*single);
        }return 62;
    }
    int CopyColorToSampled(uint64_t colorToken,uint64_t imageToken) {
        if(!colorToken || !imageToken)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        CtrColorAttachmentOwner* color=nullptr;CtrSampledImageOwner* image=nullptr;
        for(auto& e:colorAttachments_)if(e.first==colorToken)color=e.second.get();
        for(auto& e:sampledImages_)if(e.first==imageToken)image=e.second.get();
        if(!color || !image)return 62;
        const auto& c=color->requirement;const auto& p=image->parameters;
        // Exact same backing and interpretation only. No partial overlap or format reinterpretation.
        if(!c.guest_address || c.guest_address!=p.guest_address || c.data_format!=10 ||
           c.data_format!=p.data_format || c.number_format!=p.number_format || c.pitch!=p.pitch ||
           (!p.height || p.height>c.height) || c.layers!=1 || c.samples!=1 || p.layers!=1 || p.samples!=1 ||
           p.type!=9 || p.depth!=1 || p.levels!=1 || p.base_level || p.base_array || p.last_array ||
           p.compression || p.pow2pad || p.alt_tile || c.tile_mode!=p.tile_mode || !p.width || p.width>c.pitch ||
           c.pitch>4096 || c.height>4096)return 88;
        const uint64_t pitch=uint64_t(c.pitch)*4,bytes=pitch*c.height;
        std::vector<unsigned char> linear(static_cast<size_t>(bytes));
        const int copied=color->Transfer(get_,family_,memory_,nullptr,linear.data(),bytes,pitch);
        if(copied)return copied;
        return image->Upload(device_,get_,family_,memory_,linear.data(),bytes,pitch);
    }
    int UploadColorTiled(uint64_t token,const void* source,uint64_t bytes,const CtrGraphicsImageExtentV1* extent) {
        if(!token || !source || !extent || extent->version!=1 || extent->size!=sizeof(*extent))return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto& entry:colorAttachments_)if(entry.first==token) {
            const auto& p=entry.second->requirement;
            const uint32_t texelBytes=p.data_format==3?2u:4u,macroHeight=texelBytes==2?128u:64u;
            if(texelBytes==2 && !tiling16_)return 88;
            const bool display10=p.tile_mode==10 && p.data_format==10 && p.number_format==0;
            if((p.tile_mode!=14 && !display10) || p.samples!=1 || p.layers!=1 || (p.data_format!=10 && p.data_format!=3) ||
                !p.guest_address || !p.pitch || p.pitch>4096 || p.height>4096 || p.pitch%128 ||
                extent->pitch!=p.pitch || extent->padded_height%macroHeight || extent->padded_height<p.height ||
                extent->padded_height-p.height>=macroHeight || extent->guest_address!=p.guest_address)return 88;
            if(extent->bytes!=bytes || p.bytes!=bytes || bytes!=uint64_t(extent->pitch)*extent->padded_height*texelBytes)return 60;
            return ctr_with_tiling_pipelines(device_,get_,tilingDirectory_.c_str(),tilingScalar_,tilingCompute_,!display10,
                [&](VkPipelineLayout layout,const VkPipeline* pipelines,VkDescriptorSetLayout descriptors) -> int {
                    std::vector<unsigned char> linear;
                    const int converted=ctr_thin_detile(device_,get_,family_,memory_,layout,pipelines[1],descriptors,
                        extent->pitch,extent->padded_height,source,bytes,linear,false,texelBytes);
                    if(converted)return converted;
                    return entry.second->Transfer(get_,family_,memory_,linear.data(),nullptr,linear.size(),uint64_t(extent->pitch)*texelBytes);
                },texelBytes);
        }
        return 62;
    }
    int TransferColorAttachment(uint64_t token,const void* source,void* destination,uint64_t bytes,uint64_t pitch) {
        if(!token || (!source && !destination) || (source && destination))return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto& entry:colorAttachments_)if(entry.first==token)
            return entry.second->Transfer(get_,family_,memory_,source,destination,bytes,pitch);
        return 62;
    }
    int ReleaseColorAttachment(uint64_t token) {
        if(!token)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto i=colorAttachments_.begin();i!=colorAttachments_.end();++i)if(i->first==token){colorAttachments_.erase(i);return 0;}
        return 62;
    }
    int CreateDepthAttachment(const CtrDepthAttachmentParametersV1* p,uint64_t* token) {
        if(!p || !token || p->version!=1 || p->size!=sizeof(*p))return 60;
        VkImageCreateInfo info{};VkImageAspectFlags aspects=0;
        const int status=PreflightDepth({p->width,p->height,p->samples,p->z_format,p->stencil_format},info,aspects);
        if(status)return status;
        if(depthToken_==UINT64_MAX)return 88;
        depthAttachments_.reserve(depthAttachments_.size()+1);
        std::unique_ptr<CtrDepthAttachmentOwner> image;
        const int created=CtrDepthAttachmentOwner::Create(device_,get_,memory_,info,aspects,image);
        if(created)return created;
        image->z_format=p->z_format;image->stencil_format=p->stencil_format;
        const auto value=++depthToken_;depthAttachments_.emplace_back(value,std::move(image));*token=value;return 0;
    }
    int AliasSampledDepth(uint64_t depthToken,uint64_t* token) {
      if(!token || !depthToken)return 60;
      if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
      for(auto& entry:depthAttachments_)if(entry.first==depthToken){
        if(imageToken_==UINT64_MAX)return 88;
        sampledImages_.reserve(sampledImages_.size()+1);
        auto image=CtrSampledImageOwner::AliasDepth(entry.second);
        const int rc=entry.second->SetSampled(get_,family_,true);if(rc)return rc;
        const auto id=++imageToken_;sampledImages_.emplace_back(id,std::move(image));*token=id;return 0;
      }return 62;
    }
    int RestoreDepthAttachment(uint64_t token) {
      if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
      for(auto& entry:depthAttachments_)if(entry.first==token)return entry.second->SetSampled(get_,family_,false);
      return 62;
    }
    int DepthSampledControl(uint64_t token) {
      if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
      for(auto& entry:depthAttachments_)if(entry.first==token){
        auto& depth=*entry.second;VkDescriptorImageInfo descriptor{};
        if(depth.SampledDescriptor(descriptor)!=91 || !depth.Ready())return 215;
        int rc=depth.SetSampled(get_,family_,true);if(rc)return rc;
        if(depth.Ready() || depth.SampledDescriptor(descriptor) || descriptor.imageView!=depth.SampledView() ||
           descriptor.imageLayout!=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)return 215;
        if(depth.SetSampled(get_,family_,true))return 215;
        rc=depth.SetSampled(get_,family_,false);if(rc)return rc;
        if(!depth.Ready() || depth.SampledDescriptor(descriptor)!=91)return 215;
        return 0;
      }return 62;
    }
    int ReadDepthD16Tiled(uint64_t token,const CtrDepthD16LayoutV1* parameters,const void* prior,uint64_t bytes,void* output) {
        if(!token || !prior || !output)return 60;
        CtrGraphicsImageExtentV1 extent{};const int checked=CtrPm4::DepthD16Extent(parameters,&extent);if(checked)return checked;
        if(bytes!=extent.bytes)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto& entry:depthAttachments_)if(entry.first==token){
            if(entry.second->extent.width!=parameters->width || entry.second->extent.height!=parameters->height)return 88;
            return ctr_with_tiling_pipelines(device_,get_,tilingDirectory_.c_str(),tilingScalar_,tilingCompute_,true,
              [&](VkPipelineLayout layout,const VkPipeline* pipelines,VkDescriptorSetLayout descriptors)->int{
                std::vector<unsigned char> linear,tiled;
                int rc=ctr_thin_detile(device_,get_,family_,memory_,layout,pipelines[1],descriptors,extent.pitch,extent.padded_height,prior,bytes,linear,false,2);if(rc)return rc;
                rc=entry.second->ReadD16(get_,family_,memory_,linear.data(),linear.size(),uint64_t(extent.pitch)*2);if(rc)return rc;
                rc=ctr_thin_detile(device_,get_,family_,memory_,layout,pipelines[0],descriptors,extent.pitch,extent.padded_height,linear.data(),linear.size(),tiled,true,2);if(rc)return rc;
                std::memcpy(output,tiled.data(),tiled.size());return 0;
              },2,true);
        }return 62;
    }
    // Conversion only: caller supplies actual interleaved samples from a fenced
    // extraction of this retained owner. No guest publication or guard changes.
    int RetileDepthMS2Samples(uint64_t token,const CtrDepthMS2LayoutV1* parameters,
        const void* prior,uint64_t bytes,const void* samples,uint64_t sampleBytes,void* output) {
        if(!token || !prior || !samples || !output)return 60;
        CtrGraphicsImageExtentV1 extent{};const int checked=CtrPm4::DepthMS2Extent(parameters,&extent);if(checked)return checked;
        if(bytes!=extent.bytes || sampleBytes!=1920ULL*1080*2*4)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto& entry:depthAttachments_)if(entry.first==token){
            if(entry.second->extent.width!=1920 || entry.second->extent.height!=1080 ||
               entry.second->samples!=2 || entry.second->z_format!=3 || entry.second->stencil_format!=1)return 88;
            return ctr_with_tiling_pipelines(device_,get_,tilingDirectory_.c_str(),tilingScalar_,tilingCompute_,true,
              [&](VkPipelineLayout layout,const VkPipeline* pipelines,VkDescriptorSetLayout descriptors)->int{
                std::vector<unsigned char> linear,merged,tiled;
                int rc=ctr_thin_detile(device_,get_,family_,memory_,layout,pipelines[1],descriptors,extent.pitch,extent.padded_height,prior,bytes,linear,false,4,2);if(rc)return rc;
                rc=CtrPm4::MergeDepthMS2Linear(linear.data(),linear.size(),samples,sampleBytes,merged);if(rc)return rc;
                rc=ctr_thin_detile(device_,get_,family_,memory_,layout,pipelines[0],descriptors,extent.pitch,extent.padded_height,merged.data(),merged.size(),tiled,true,4,2);if(rc)return rc;
                if(tiled.size()!=bytes)return 112;
                std::memcpy(output,tiled.data(),tiled.size());return 0;
              },4,false,2);
        }return 62;
    }
    int ReadDepthD16(uint64_t token,void* destination,uint64_t bytes,uint64_t pitch) {
        if(!token || !destination)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto& entry:depthAttachments_)if(entry.first==token)return entry.second->ReadD16(get_,family_,memory_,destination,bytes,pitch);
        return 62;
    }
    int ClearDepthAttachment(uint64_t token,uint32_t depthBits,uint32_t stencil) {
        if(!token)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto& entry:depthAttachments_)if(entry.first==token)return entry.second->Clear(get_,family_,depthBits,stencil);
        return 62;
    }
    int RegisterDepthResidency(uint64_t token,const CtrDepthMS2LayoutV1* layout,uint64_t stencil,uint64_t* generation,bool single=false) {
        if(!token || !generation)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        CtrDepthBackingSpansV1 spans{};int rc=single?CtrPm4::DepthSingleSpans(layout,stencil,&spans):CtrPm4::DepthMS2Spans(layout,stencil,&spans);if(rc)return rc;
        std::shared_ptr<CtrDepthAttachmentOwner> owner;
        for(const auto& entry:depthAttachments_)if(entry.first==token){owner=entry.second;break;}
        if(!owner)return 62;
        const bool d16=single && layout->z_format==1 && !layout->stencil_format;
        if(owner->extent.width!=layout->width || owner->extent.height!=layout->height || owner->samples!=(single?1u:2u) || owner->z_format!=(d16?1u:3u) || owner->stencil_format!=(d16?0u:1u))return 88;
        if(!owner->Ready() || owner->resident)return 91;
        if(depthGeneration_==UINT64_MAX)return 88;
        const auto overlaps=[](uint64_t a,uint64_t n,uint64_t b,uint64_t m){return n && m && a<b+m && b<a+n;};
        for(const auto& r:depthResidents_){const auto& s=r.spans;
            if(overlaps(spans.depth_address,spans.depth_bytes,s.depth_address,s.depth_bytes) ||
               overlaps(spans.depth_address,spans.depth_bytes,s.stencil_address,s.stencil_bytes) ||
               overlaps(spans.stencil_address,spans.stencil_bytes,s.depth_address,s.depth_bytes) ||
               overlaps(spans.stencil_address,spans.stencil_bytes,s.stencil_address,s.stencil_bytes))return 88;
        }
        const auto next=depthGeneration_+1;
        depthResidents_.push_back({next,spans,owner});owner->resident=true;
        depthGeneration_=next;*generation=next;return 0;
    }
    int AcquireDepthResidency(const CtrDepthBackingSpansV1* spans,uint64_t generation,uint64_t* lease) {
        if(!spans || !lease || !generation || spans->version!=1 || spans->size!=sizeof(*spans))return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(const auto& r:depthResidents_)if(r.generation==generation){
            if(std::memcmp(spans,&r.spans,sizeof(*spans)))return 88;
            if(depthToken_==UINT64_MAX)return 88;
            const auto next=depthToken_+1;depthAttachments_.emplace_back(next,r.owner);
            depthToken_=next;*lease=next;return 0;
        }
        return 62;
    }
    int RemoveDepthResidency(uint64_t generation) {
        if(!generation)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto i=depthResidents_.begin();i!=depthResidents_.end();++i)if(i->generation==generation){i->owner->resident=false;depthResidents_.erase(i);return 0;}
        return 62;
    }
    int RetainDepthAttachment(uint64_t token,uint64_t* lease) {
        if(!token || !lease)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(depthToken_==UINT64_MAX)return 88;
        std::shared_ptr<CtrDepthAttachmentOwner> owner;
        for(const auto& entry:depthAttachments_)if(entry.first==token){owner=entry.second;break;}
        if(!owner)return 62;
        depthAttachments_.reserve(depthAttachments_.size()+1);
        const uint64_t retained=depthToken_+1;
        depthAttachments_.emplace_back(retained,std::move(owner));
        depthToken_=retained;*lease=retained;return 0;
    }
    int ReleaseDepthAttachment(uint64_t token) {
        if(!token)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto i=depthAttachments_.begin();i!=depthAttachments_.end();++i)if(i->first==token){depthAttachments_.erase(i);return 0;}
        return 62;
    }
    int CreateBuffer(const void* source,uint64_t bytes,uint32_t usage,uint64_t* token) {
        if(!source || !token || !bytes || bytes>32ULL*1024*1024)return 60;
        if(!window_ || !IsWindow(window_) || !samplerAuthority_)return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(usage<1 || usage>4 || bufferToken_==UINT64_MAX)return 88;
        if(usage==1 && bytes>samplerLimits_.maxStorageBufferRange)return 88;
        if((usage==2 && bytes%2) || (usage==3 && bytes%4))return 60;
        buffers_.reserve(buffers_.size()+1);
        std::unique_ptr<CtrOwnedBuffer> buffer;
        const auto flags=usage==1 ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : (usage==4 ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        const int status=CtrOwnedBuffer::Create(device_,get_,memory_,source,bytes,flags,buffer,usage==2?2u:usage==3?4u:0u);
        if(status)return status;
        const auto value=++bufferToken_;buffers_.emplace_back(value,std::move(buffer));*token=value;return 0;
    }
    int CopyBufferInitial(uint64_t token,void* destination,uint64_t bytes) {
        if(!token || !destination)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto& entry:buffers_)if(entry.first==token)return entry.second->CopyInitialized(destination,bytes);
        return 62;
    }
    int ReleaseBuffer(uint64_t token) {
        if(!token)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto i=buffers_.begin();i!=buffers_.end();++i)if(i->first==token){buffers_.erase(i);return 0;}
        return 62;
    }
    int CreateStorageImage(const CtrGraphicsImageParametersV1* p,uint64_t* token){
        if(!p || !token || p->version!=1 || p->size!=sizeof(*p) || !p->width || !p->height)return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(!imagePhysical_ || !imageFormatQuery_ || !imageLimitsQuery_)return 91;
        if(p->data_format==3 && !tiling16_)return 88;
        const VkFormat storageFormat=p->data_format==4 && p->number_format==7?VK_FORMAT_R32_SFLOAT:p->data_format==10 && p->number_format==0?VK_FORMAT_R8G8B8A8_UNORM:p->data_format==3 && p->number_format==0?VK_FORMAT_R8G8_UNORM:VK_FORMAT_UNDEFINED;
        if(p->type!=9 || storageFormat==VK_FORMAT_UNDEFINED || p->depth!=1 || p->layers!=1 || p->levels!=1 || p->samples!=1 ||
           p->compression || p->alt_tile || p->base_array || p->last_array || p->base_level)return 88;
        VkFormatProperties props{};imageFormatQuery_(imagePhysical_,storageFormat,&props);
        const auto features=VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT|VK_FORMAT_FEATURE_TRANSFER_SRC_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
        if((props.optimalTilingFeatures&features)!=features)return 88;
        const VkImageUsageFlags usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkImageFormatProperties limits{};
        if(imageLimitsQuery_(imagePhysical_,storageFormat,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,usage,0,&limits)!=VK_SUCCESS ||
           p->width>limits.maxExtent.width || p->height>limits.maxExtent.height || !(limits.sampleCounts&VK_SAMPLE_COUNT_1_BIT))return 88;
        if(imageToken_==UINT64_MAX)return 88;
        VkImageCreateInfo info{};info.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;info.imageType=VK_IMAGE_TYPE_2D;info.format=storageFormat;
        info.extent={p->width,p->height,1};info.mipLevels=info.arrayLayers=1;info.samples=VK_SAMPLE_COUNT_1_BIT;info.tiling=VK_IMAGE_TILING_OPTIMAL;
        info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;info.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        sampledImages_.reserve(sampledImages_.size()+1);std::unique_ptr<CtrSampledImageOwner> image;
        VkComponentMapping components{};const int status=CtrSampledImageOwner::Create(device_,get_,memory_,info,components,image);if(status)return status;
        image->parameters=*p;const auto id=++imageToken_;sampledImages_.emplace_back(id,std::move(image));*token=id;return 0;
    }
    int CreateSampledImage(const CtrGraphicsImageParametersV1* parameters,uint64_t* token) {
        if(!parameters || !token)return 60;
        if(!window_ || !IsWindow(window_) || !imagePhysical_)return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        VkImageCreateInfo info{};
        const int status=CtrPm4::PreflightSampledImage(*parameters,imagePhysical_,imageFormatQuery_,imageLimitsQuery_,info);
        if(status)return status;
        VkComponentMapping components{};
        const int mapped=CtrPm4::SampledRgba8Components(parameters->swizzle,components);
        if(mapped)return mapped;
        if(imageToken_==UINT64_MAX)return 88;
        sampledImages_.reserve(sampledImages_.size()+1);
        std::unique_ptr<CtrSampledImageOwner> image;
        const int created=CtrSampledImageOwner::Create(device_,get_,memory_,info,components,image);
        if(created)return created;
        image->parameters=*parameters;
        const auto value=++imageToken_;sampledImages_.emplace_back(value,std::move(image));*token=value;return 0;
    }
    int UploadSampledImage(uint64_t token,const void* source,uint64_t bytes,uint64_t pitch) {
        if(!token || !source)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto& entry:sampledImages_)if(entry.first==token)
            return entry.second->Upload(device_,get_,family_,memory_,source,bytes,pitch);
        return 62;
    }
    int ReadbackSampledImage(uint64_t token,void* destination,uint64_t bytes,uint64_t pitch) {
        if(!token || !destination)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto& entry:sampledImages_)if(entry.first==token)
            return entry.second->Readback(device_,get_,family_,memory_,destination,bytes,pitch);
        return 62;
    }
    int TransferBc3Tiled(CtrSampledImageOwner& image,const void* source,uint64_t bytes,
        const CtrGraphicsImageExtentV1& extent,void* output) {
        const auto& p=image.parameters;
        const uint32_t blockBytes=p.data_format==35?8u:16u;
        std::vector<uint32_t> pitches,heights;
        uint64_t total=0;
        for(uint32_t mip=0;mip<p.levels;++mip) {
            uint32_t w=std::max(((p.pitch>>mip)+3)/4,1u),h=std::max(((p.height>>mip)+3)/4,1u);
            if(p.pow2pad){uint32_t a=1,b=1;while(a<w)a*=2;while(b<h)b*=2;w=a;h=b;}
            w=(w+7)&~7u;h=(h+7)&~7u;pitches.push_back(w);heights.push_back(h);
            total+=uint64_t(w)*h*blockBytes*p.layers;
        }
        if(pitches.empty() || extent.pitch!=pitches[0]*4 || extent.padded_height!=heights[0]*4 ||
           extent.guest_address!=p.guest_address)return 88;
        if(bytes!=total || extent.bytes!=total)return 60;
        return ctr_with_tiling_pipelines(device_,get_,tilingDirectory_.c_str(),tilingScalar_,tilingCompute_,true,
          [&](VkPipelineLayout layout,const VkPipeline* pipelines,VkDescriptorSetLayout descriptors)->int {
            std::vector<std::vector<unsigned char>> levels(size_t(p.levels)*p.layers);size_t offset=0;
            for(uint32_t mip=0;mip<p.levels;++mip) for(uint32_t layer=0;layer<p.layers;++layer) {
                const size_t index=size_t(mip)*p.layers+layer;
                const size_t size=size_t(pitches[mip])*heights[mip]*blockBytes;
                const int rc=ctr_thin_detile(device_,get_,family_,memory_,layout,pipelines[1],descriptors,
                    pitches[mip],heights[mip],static_cast<const unsigned char*>(source)+offset,size,levels[index],false,blockBytes,1,true);
                if(rc)return rc;offset+=size;
            }
            const int rc=image.TransferBc3(device_,get_,family_,memory_,levels,pitches,output!=nullptr);if(rc)return rc;
            if(output) {
                std::vector<unsigned char> combined;combined.reserve(size_t(total));
                for(uint32_t mip=0;mip<p.levels;++mip) for(uint32_t layer=0;layer<p.layers;++layer) {
                    const size_t index=size_t(mip)*p.layers+layer;
                    std::vector<unsigned char> tiled;
                    const int result=ctr_thin_detile(device_,get_,family_,memory_,layout,pipelines[0],descriptors,
                        pitches[mip],heights[mip],levels[index].data(),levels[index].size(),tiled,true,blockBytes,1,true);
                    if(result)return result;combined.insert(combined.end(),tiled.begin(),tiled.end());
                }
                std::memcpy(output,combined.data(),combined.size());
            }
            return 0;
          },blockBytes,false,1,true);
    }
    int UploadSampledTiled(uint64_t token,const void* source,uint64_t bytes,const CtrGraphicsImageExtentV1* extent) {
        if(!token || !source || !extent || extent->version!=1 || extent->size!=sizeof(*extent))return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto& entry:sampledImages_)if(entry.first==token) {
            const auto& p=entry.second->parameters;
            if(p.data_format==37 || p.data_format==35)return TransferBc3Tiled(*entry.second,source,bytes,*extent,nullptr);
            if(p.data_format==1){
                if(p.tile_mode!=8 || extent->pitch!=p.pitch || extent->padded_height!=p.height || extent->guest_address!=p.guest_address ||
                   bytes!=uint64_t(p.pitch)*p.height || extent->bytes!=bytes)return 88;
                return entry.second->Upload(device_,get_,family_,memory_,source,bytes,p.pitch);
            }
            const uint32_t texelBytes=(p.data_format==2 || p.data_format==3)?2u:4u,macroHeight=p.tile_mode==13?8u:p.tile_mode==0?128u:texelBytes==2?128u:64u;
            const bool micro13=p.tile_mode==13 && p.type==9 && p.samples==1 &&
                ((p.data_format==5 && p.number_format==7 && p.width==64 && p.height==64 && p.pitch==64) ||
                 (p.data_format==10 && p.number_format==0));
            const bool display10=p.tile_mode==10 && p.data_format==10 && p.number_format==0 && p.type==9 && p.samples==1;
            const bool depth0=p.tile_mode==0 && ((p.data_format==4 && p.number_format==7) || (p.data_format==2 && p.number_format==0 && p.width==2048 && p.height==8192 && p.pitch==2048)) && p.samples==1;
            if(texelBytes==2 && !tiling16_)return 88;
            if((p.tile_mode!=14 && !depth0 && !micro13 && !display10) || p.pow2pad || p.alt_tile || p.compression || p.base_level ||
                p.base_array || p.last_array || !p.pitch || p.pitch>4096 || p.pitch<p.width ||
                (p.height>4096 && !(depth0 && p.data_format==2)) || !p.guest_address)return 88;
            // Public tile14/sample1 macro extent:32bpp128x64;16bpp128x128. Reject unaligned
            // source pitches rather than treating them as the padded stride supplied by caller.
            if(p.pitch%(micro13?8u:depth0?256u:128u) || extent->pitch!=p.pitch || extent->padded_height%macroHeight ||
                extent->padded_height<p.height || extent->padded_height-p.height>=macroHeight ||
                extent->guest_address!=p.guest_address)return 88;
            if(extent->bytes!=bytes || bytes!=uint64_t(extent->pitch)*extent->padded_height*texelBytes)return 60;
            return ctr_with_tiling_pipelines(device_,get_,tilingDirectory_.c_str(),tilingScalar_,tilingCompute_,!display10,
                [&](VkPipelineLayout layout,const VkPipeline* pipelines,VkDescriptorSetLayout descriptors) -> int {
                    std::vector<unsigned char> linear;
                    const int converted=ctr_thin_detile(device_,get_,family_,memory_,layout,pipelines[1],descriptors,
                        extent->pitch,extent->padded_height,source,bytes,linear,false,texelBytes,1,micro13,depth0 && p.data_format==2);
                    if(converted)return converted;
                    return entry.second->Upload(device_,get_,family_,memory_,linear.data(),linear.size(),uint64_t(extent->pitch)*texelBytes);
                },texelBytes,depth0,1,micro13);
        }
        return 62;
    }
    int ReadbackImageTiled(uint64_t token,const void* prior,uint64_t bytes,const CtrGraphicsImageExtentV1* extent,void* output){
        if(!token || !prior || !output || !extent || extent->version!=1 || extent->size!=sizeof(*extent))return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto& entry:sampledImages_)if(entry.first==token){
            const auto& p=entry.second->parameters;
            if(p.data_format==37 || p.data_format==35)return TransferBc3Tiled(*entry.second,prior,bytes,*extent,output);
            const uint32_t texelBytes=(p.data_format==2 || p.data_format==3)?2u:4u,macroHeight=p.tile_mode==13?8u:p.tile_mode==0?128u:texelBytes==2?128u:64u;
            const bool micro13=p.tile_mode==13 && p.type==9 && p.samples==1 &&
                ((p.data_format==5 && p.number_format==7 && p.width==64 && p.height==64 && p.pitch==64) ||
                 (p.data_format==10 && p.number_format==0));
            const bool display10=p.tile_mode==10 && p.data_format==10 && p.number_format==0 && p.type==9 && p.samples==1;
            const bool depth0=p.tile_mode==0 && ((p.data_format==4 && p.number_format==7) || (p.data_format==2 && p.number_format==0 && p.width==2048 && p.height==8192 && p.pitch==2048)) && p.samples==1;
            if(texelBytes==2 && !tiling16_)return 88;
            if((p.tile_mode!=14 && !depth0 && !micro13 && !display10) || p.pow2pad || p.alt_tile || p.compression || p.base_level || p.base_array || p.last_array ||
               !p.pitch || p.pitch>4096 || p.pitch<p.width || (p.height>4096 && !(depth0 && p.data_format==2)) || !p.guest_address)return 88;
            if(p.pitch%(micro13?8u:depth0?256u:128u) || extent->pitch!=p.pitch || extent->padded_height%macroHeight || extent->padded_height<p.height ||
               extent->padded_height-p.height>=macroHeight || extent->guest_address!=p.guest_address)return 88;
            if(extent->bytes!=bytes || bytes!=uint64_t(extent->pitch)*extent->padded_height*texelBytes)return 60;
            return ctr_with_tiling_pipelines(device_,get_,tilingDirectory_.c_str(),tilingScalar_,tilingCompute_,!display10,
              [&](VkPipelineLayout layout,const VkPipeline* pipelines,VkDescriptorSetLayout descriptors)->int{
                std::vector<unsigned char> linear,tiled;
                int status=ctr_thin_detile(device_,get_,family_,memory_,layout,pipelines[1],descriptors,extent->pitch,extent->padded_height,prior,bytes,linear,false,texelBytes,1,micro13,depth0 && p.data_format==2);if(status)return status;
                status=entry.second->Readback(device_,get_,family_,memory_,linear.data(),linear.size(),uint64_t(extent->pitch)*texelBytes);if(status)return status;
                status=ctr_thin_detile(device_,get_,family_,memory_,layout,pipelines[0],descriptors,extent->pitch,extent->padded_height,linear.data(),linear.size(),tiled,true,texelBytes,1,micro13,depth0 && p.data_format==2);if(status)return status;
                std::memcpy(output,tiled.data(),tiled.size());return 0;
              },texelBytes,depth0,1,micro13);
        }return 62;
    }
    int ReleaseSampledImage(uint64_t token) {
        if(!token)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto i=sampledImages_.begin();i!=sampledImages_.end();++i)if(i->first==token) {
            // Upload is synchronous and drains failed submissions before returning.
            sampledImages_.erase(i);return 0;
        }
        return 62;
    }
    int CreateSampler(const CtrGraphicsSamplerParametersV1* parameters,uint64_t* token) {
        if(!parameters || !token)return 60;
        if(!window_ || !IsWindow(window_) || !samplerAuthority_)return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        VkSamplerCreateInfo info{};
        const int status=CtrPm4::PreflightSampler(*parameters,samplerLimits_,false,info);
        if(status)return status;
        const auto create=reinterpret_cast<PFN_vkCreateSampler>(get_(device_,"vkCreateSampler"));
        const auto destroy=reinterpret_cast<PFN_vkDestroySampler>(get_(device_,"vkDestroySampler"));
        if(!create || !destroy || samplerToken_==UINT64_MAX)return 88;
        // Reserve before Vulkan allocation so allocation failure cannot leak a handle.
        samplers_.reserve(samplers_.size()+1);
        auto sampler=std::make_shared<CtrOwnedSampler>();sampler->device=device_;sampler->destroy=destroy;
        if(create(device_,&info,nullptr,&sampler->sampler)!=VK_SUCCESS)return 90;
        const auto value=++samplerToken_;samplers_.emplace_back(value,std::move(sampler));*token=value;return 0;
    }
    int ReleaseSampler(uint64_t token) {
        if(!token)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        for(auto i=samplers_.begin();i!=samplers_.end();++i)if(i->first==token) {
            samplers_.erase(i);return 0;
        }
        return 62;
    }
    int HideSystemSplash() {
        if(!window_ || !IsWindow(window_) || !device_ || !swap_ || !extent_.width || !extent_.height ||
           !closeRequested_ || *closeRequested_)return 91;
        DWORD process=0;const DWORD thread=GetWindowThreadProcessId(window_,&process);
        if(thread!=GetCurrentThreadId() || process!=GetCurrentProcessId())return 116;
        // There is no console compositor splash in this runtime. Its owned
        // presentation HWND is the genuine lifecycle authority to expose.
        RECT client{};if(!GetClientRect(window_,&client) || client.right<=client.left || client.bottom<=client.top)return 91;
        if(!IsWindowVisible(window_))ShowWindow(window_,SW_SHOWNOACTIVATE);
        if(!IsWindowVisible(window_))return 91;
        if(!UpdateWindow(window_))return 91;
        splashHidden_=true;
        return 0;
    }
    int SplashLifecycleControl() {
        if(!window_ || !IsWindow(window_))return 91;
        const bool wasVisible=IsWindowVisible(window_)!=FALSE;
        ShowWindow(window_,SW_HIDE);
        if(IsWindowVisible(window_) || HideSystemSplash() || !splashHidden_ || !IsWindowVisible(window_) || HideSystemSplash())return 154;
        *closeRequested_=true;const int closed=HideSystemSplash();*closeRequested_=false;
        int workerStatus=0;std::thread worker([&]{workerStatus=HideSystemSplash();});worker.join();
        if(!wasVisible)ShowWindow(window_,SW_HIDE);
        if(closed!=91 || workerStatus!=116)return 154;
        std::puts("system splash lifecycle passed actual_HWND_revealed=true idempotent=true closed91=true wrongthread116=true no_gameframe_credit=true");
        return 0;
    }
    int RegisterTiled(const std::shared_ptr<ctrport::DirectMemoryBacking>& owner,
        uint32_t first,uint32_t count,const CtrVideoBufferAttributesV1& attributes,
        const uint64_t* addresses,uint32_t& group) {
        if(eventError_) return eventError_;
        // Distinct implemented paths cannot alias public slot/group identities.
        for(const auto& entry:groups_) if(entry.occupied) return 91;
        for(const auto& slot:slots_) if(slot) return 91;
        if(!tiled_) tiled_=std::make_unique<CtrTiledSurfaceRegistry>();
        return tiled_->RegisterMapped(device_,get_,memory_,first,count,attributes,addresses,owner,group);
    }
    int SubmitTiledSnapshot(uint32_t slot,uint64_t address,const void* source,uint64_t bytes,uint64_t argument) {
        if(!source || slot>=16)return 60;
        if(presentationGamma_!=1.0f)return 89;
        if(!window_ || !IsWindow(window_))return 91;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(eventError_)return eventError_;
        const auto* surface=tiled_?tiled_->Surface(slot):nullptr;
        if(!surface)return 62;
        if(address!=surface->guestAddress || bytes!=surface->guestBytes)return 88;
        if(pending_.size()>=16)return 63;
        return ctr_with_tiling_pipelines(device_,get_,tilingDirectory_.c_str(),tilingScalar_,tilingCompute_,false,
          [&](VkPipelineLayout layout,const VkPipeline* pipelines,VkDescriptorSetLayout descriptors)->int{
            std::vector<unsigned char> linear;
            const int converted=ctr_thin_detile(device_,get_,family_,memory_,layout,pipelines[1],descriptors,
              surface->paddedPitch,surface->paddedHeight,source,bytes,linear);
            if(converted)return converted;
            // Presentation scales the full registered source to the actual swapchain client extent.
            // Nearest texel selection preserves source channel values; no invented game content.
            std::vector<unsigned char> scaled(size_t(extent_.width)*extent_.height*4);
            for(uint32_t y=0;y<extent_.height;++y)for(uint32_t x=0;x<extent_.width;++x){
              const size_t sourceOffset=(size_t(uint64_t(y)*surface->height/extent_.height)*surface->paddedPitch+
                uint64_t(x)*surface->width/extent_.width)*4;
              std::memcpy(scaled.data()+(size_t(y)*extent_.width+x)*4,linear.data()+sourceOffset,4);
            }
            const bool hostBgra=format_==VK_FORMAT_B8G8R8A8_UNORM || format_==VK_FORMAT_B8G8R8A8_SRGB;
            if(!hostBgra && format_!=VK_FORMAT_R8G8B8A8_UNORM && format_!=VK_FORMAT_R8G8B8A8_SRGB)return 88;
            if(!hostBgra)for(size_t pixel=0;pixel<scaled.size();pixel+=4)std::swap(scaled[pixel],scaled[pixel+2]);
            if(!slots_[slot] && nextToken_==UINT64_MAX)return 98;
            auto frame=std::make_shared<HostFrame>();frame->owner=identity_;frame->slot=slot;
            frame->token=slots_[slot]?slots_[slot]->token:nextToken_+1;
            frame->pitch=size_t(extent_.width)*4;frame->bytes=std::move(scaled);
            if(!EnqueueHostFrame(frame,argument))return 63;
            if(!slots_[slot])++nextToken_;
            slots_[slot]=frame;return 0;
          });
    }
    int CompleteTiledFlip(uint32_t slot,const CtrVideoCompletionV1* completion){
        if(!completion || slot>=16)return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(!tiled_ || !tiled_->Surface(slot))return 62;
        if(!completion->count || completion->count!=completed_ || completion->token!=lastToken_ ||
          completion->argument!=lastArgument_ || lastSlot_!=int64_t(slot))return 88;
        if(completion->count<=tiledCompletion_)return 91;
        if(completion->count!=tiledCompletion_+1)return 88;
        std::array<uint64_t,16> labels{};uint64_t labelGeneration=0;
        const int read=tiled_->LabelSnapshot(labels,labelGeneration);if(read)return read;
        if(labels[slot]!=1)return 88;
        if(tiledPreviousSlot_>=0){
          if(!tiled_->Surface(static_cast<uint32_t>(tiledPreviousSlot_)))return 62;
          const int retired=tiled_->PublishLabel(static_cast<uint32_t>(tiledPreviousSlot_),0);if(retired)return retired;
        }
        tiledPreviousSlot_=static_cast<int32_t>(slot);tiledCompletion_=completion->count;return 0;
    }
    // Bounded synchronous frame boundary: all provider work is submitted synchronously.
    // Guest DCB/CCB offsets, init packets and frame numbers remain runtime-owned.
    int EndTiledFrame(const CtrVideoCompletionV1* completion,uint64_t* frame) {
        if(!completion || !frame)return 60;
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(!completion->count || completion->count!=completed_ || completion->token!=lastToken_ ||
          completion->argument!=lastArgument_ || completion->count!=tiledCompletion_)return 88;
        if(completion->count<=endedCompletion_)return 91;
        if(!pending_.empty())return 63;
        if(endedFrame_==UINT64_MAX)return 98;
        const int barrier=QueueMemoryVisibility();if(barrier)return barrier;
        const auto idle=reinterpret_cast<PFN_vkDeviceWaitIdle>(get_(device_,"vkDeviceWaitIdle"));
        if(!idle || idle(device_)!=VK_SUCCESS)return 91;
        endedCompletion_=completion->count;*frame=++endedFrame_;return 0;
    }
    int UnregisterTiled(uint32_t group) {
        if(!tiled_)return 62;
        std::array<const CtrTiledSurface*,16> previous{};
        for(uint32_t slot=0;slot<16;++slot)previous[slot]=tiled_->Surface(slot);
        const int status=tiled_->Unregister(group);if(status)return status;
        // Pending requests own immutable host snapshots and survive slot removal.
        for(uint32_t slot=0;slot<16;++slot)if(previous[slot] && !tiled_->Surface(slot))slots_[slot].reset();
        return 0;
    }
    int TiledLabelAddress(uint64_t& address) {
        if(!tiled_) return 91;
        address=tiled_->LabelAddress();return 0;
    }
    void SetGraphicsProfile(const CtrGraphicsProfileV1& profile,int status) {
        shaderProfile_=profile;shaderProfileStatus_=status;
    }
    int CaptureGraphicsProfile(CtrGraphicsProfileV1* output) const {
        if(!output)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        DWORD process=0;const DWORD thread=GetWindowThreadProcessId(window_,&process);
        if(thread!=GetCurrentThreadId() || process!=GetCurrentProcessId())return 116;
        if(shaderProfileStatus_)return shaderProfileStatus_;
        *output=shaderProfile_;return 0;
    }
    int CapturePm4State(uint32_t* flat,uint32_t flatWords,uint32_t* compute,uint32_t computeWords,uint64_t* completed) const {
        if(!flat || !compute || !completed || flatWords!=0xD000 || computeWords!=80)return 60;
        if(!window_ || !IsWindow(window_))return 91;
        DWORD process=0;const DWORD thread=GetWindowThreadProcessId(window_,&process);
        if(thread!=GetCurrentThreadId() || process!=GetCurrentProcessId())return 116;
        if(pm4Failed_ || eventError_ || !pm4Completed_)return 91;
        std::copy(pm4Registers_.words.begin(),pm4Registers_.words.end(),flat);
        std::copy(pm4Registers_.compute.begin(),pm4Registers_.compute.end(),compute);
        *completed=pm4Completed_;return 0;
    }
    int SubmitInit(const std::shared_ptr<ctrport::DirectMemoryBacking>& owner,const uint32_t* data,size_t words) {
        if(pm4Failed_ || eventError_) return 91;
        if(!owner) return 60;
        if(!pm4PendingPlan_.empty()) {
            if(owner!=pm4PendingOwner_ || words!=pm4PendingInput_.size() || !data ||
               !std::equal(pm4PendingInput_.begin(),pm4PendingInput_.end(),data))return 91;
        } else {
            std::vector<CtrPm4::InitOperation> plan;
            const int prepared=CtrPm4::PrepareInit(data,words,plan);if(prepared)return prepared;
            std::vector<std::shared_ptr<ctrport::DirectMappingLease>> leases(plan.size());
            auto labels=tiled_?tiled_->RetainLabelMemory():nullptr;
            // Pin every memory wait AND EOP before the first state/queue effect.
            for(size_t i=0;i<plan.size();++i) {
                const auto& op=plan[i];const auto& p=op.payload;
                uint64_t address=0,bytes=0;
                if(op.opcode==0x47){address=p[1]|(uint64_t(p[2]&65535)<<32);bytes=8;}
                if(op.opcode==0x3c && (p[0]&16u)){address=p[1]|(uint64_t(p[2])<<32);bytes=4;}
                if(op.opcode==0x3c && bytes && labels && address>=labels->address() &&
                   address-labels->address()<=124)continue; // Exact retained128B label owner.
                if(bytes)try{leases[i]=std::make_shared<ctrport::DirectMappingLease>(owner,address,bytes);}
                    catch(...){return 112;}
            }
            pm4PendingInput_.assign(data,data+words);pm4PendingPlan_=std::move(plan);
            pm4PendingLeases_=std::move(leases);pm4PendingLabels_=std::move(labels);pm4PendingOwner_=owner;pm4PendingCursor_=0;
        }
        while(pm4PendingCursor_<pm4PendingPlan_.size()) {
            const auto& operation=pm4PendingPlan_[pm4PendingCursor_];
            int status=0;const auto& p=operation.payload;
            switch(operation.opcode) {
            case 0x10:break;
            case 0x28:pm4LoadControl_=p[0];pm4ShadowControl_=p[1];break;
            case 0x12:case 0x2f:case 0x69:case 0x76:case 0x79:
                status=CtrPm4::ApplyRegister(operation,pm4Registers_);break;
            case 0x58:status=ctr_pm4_queue_barrier(device_,get_,family_);break;
            case 0x3c: {
                uint32_t value=0;
                if(p[0]&16u) {
                    const auto& lease=pm4PendingLeases_[pm4PendingCursor_];
                    int readStatus=1;
                    if(lease)readStatus=lease->read(0,&value,4);
                    else if(pm4PendingLabels_) {
                        auto memory=pm4PendingLabels_->api();
                        readStatus=memory.read(memory.context,p[1]|(uint64_t(p[2])<<32),&value,4);
                    }
                    if(readStatus){status=112;break;}
                }else value=pm4Registers_.words[p[1]];
                const uint32_t masked=value&p[4],reference=p[3];bool passed=false;
                // Whole public PM4CmdWaitRegMem::Test, pinned pm4_cmds.h643-713.
                switch(p[0]&7u){case 0:passed=true;break;case 1:passed=masked<reference;break;
                case 2:passed=masked<=reference;break;case 3:passed=masked==reference;break;
                case 4:passed=masked!=reference;break;case 5:passed=masked>=reference;break;
                case 6:passed=masked>reference;break;default:status=88;break;}
                if(!status && !passed)return 88; // Retain exact cursor, never replay prefix.
                break;
            }
            case 0x47: {
                status=ctr_pm4_queue_barrier(device_,get_,family_);
                const uint64_t value=p[3]|(uint64_t(p[4])<<32);
                if(!status && pm4PendingLeases_[pm4PendingCursor_]->store_u64_release(0,value))status=112;
                break;
            }
            default:status=88;break;
            }
            if(status){pm4Failed_=true;return status;}
            ++pm4PendingCursor_;
        }
        pm4PendingPlan_.clear();pm4PendingInput_.clear();pm4PendingLeases_.clear();pm4PendingOwner_.reset();pm4PendingLabels_.reset();
        ++pm4Completed_;return 0;
    }
    int AdmitMemoryChange(uint64_t address,uint64_t bytes) const {
        if(pm4Failed_ || eventError_) return 91;
        if(!bytes || address>UINT64_MAX-bytes) return 60;
        // Host frames are copied snapshots; initialization retains registers only.
        // Every current guest-backed GPU resource is in the tiled registry.
        // Future caches must join this check before their first registration.
        return tiled_ ? tiled_->AdmitMemoryChange(address,bytes) : 0;
    }
    int SetFlipRate(int32_t rate) {
        if(eventError_) return eventError_;
        // Bounded supported domain; public signed extremes remain unsupported.
        if(rate<0 || rate>2) return 88;
        flipRate_=rate;return 0;
    }
    int ConfigureOutputAny(uint32_t reserved,const CtrVideoOutputModeV1& mode,
        const void* options,uint32_t sizeMode,uint32_t sizeOptions) const {
        if(eventError_) return eventError_;
        if(reserved || sizeMode!=32 || mode.size!=32) return 60;
        if(options || sizeOptions!=16) return 88;
        if(mode.encoding!=255 || mode.range!=255 || mode.colorimetry!=255 || mode.depth!=255 ||
           mode.refresh_rate!=UINT64_MAX || mode.resolution!=UINT64_MAX) return 88;
        for(const auto byte:mode.reserved) if(byte!=255) return 88;
        if(!extent_.width || !extent_.height || !device_ || !swap_) return 91;
        uint64_t capability=UINT64_MAX;
        // Any selects the already active SDR output. Its format/colorspace are
        // checked on every request; no synthetic display configuration is applied.
        return PublicCapability(capability);
    }
    int PublicCapability(uint64_t& capability) const {
        const bool sdr=format_==VK_FORMAT_B8G8R8A8_UNORM || format_==VK_FORMAT_B8G8R8A8_SRGB ||
                       format_==VK_FORMAT_R8G8B8A8_UNORM || format_==VK_FORMAT_R8G8B8A8_SRGB;
        if(!sdr || colorSpace_!=VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return 88;
        // Actual active SDR swapchain cannot expose BT2020/PQ output. No HDR claim.
        capability=0;return 0;
    }
    PresentationSession(const PresentationSession&) = delete;
    PresentationSession& operator=(const PresentationSession&) = delete;
    ~PresentationSession() {
        StopPacing();

        samplers_.clear();
        computePipelines_.clear();graphicsPipelines_.clear();
        graphicsLayouts_.clear();
        colorAttachments_.clear();
        for(auto& r:depthResidents_)r.owner->resident=false;
        depthResidents_.clear();
        depthAttachments_.clear();
        sampledImages_.clear();
        buffers_.clear();
        tiled_.reset(); // Release GPU images before destroying their Vulkan device.
        if(events_.Clear()) std::fprintf(stderr,"VideoOut event detach failed\n");
        // Each Present waits idle before returning, including failure paths.
        destroySwap_(device_, swap_, nullptr); destroyDevice_(device_, nullptr);
    }
    struct HostBufferInput { const void* bytes; size_t length,pitch; };
    // Host snapshot groups only: no guest addresses, labels or attribute conversion.
    // Admission is atomic on this owner thread. Failed allocation may consume token IDs.
    int RegisterHostGroup(uint32_t first, uint32_t count, const HostBufferInput* inputs, uint32_t& output) {
        if(!inputs || !count || first>=16 || count>16-first) return 60;
        if(tiled_) return 91;
        uint32_t group=0;
        while(group<groups_.size() && groups_[group].occupied) ++group;
        if(group==groups_.size()) return 108;
        for(uint32_t n=0;n<count;++n) if(slots_[first+n]) return 109;
        uint32_t installed=0;
        try {
            for(;installed<count;++installed) {
                const auto& input=inputs[installed];
                if(!RegisterHostFrame(first+installed,input.bytes,input.length,input.pitch)) {
                    for(uint32_t n=0;n<installed;++n) slots_[first+n].reset();
                    return 61;
                }
            }
        } catch(...) {
            for(uint32_t n=0;n<installed;++n) slots_[first+n].reset();
            throw;
        }
        groups_[group]={true,first,count,false,{}}; output=group; return 0;
    }
    int RegisterLinearGroup(uint32_t first,uint32_t count,const CtrVideo::DisplayBufferAttributes& attributes,
                            const HostBufferInput* inputs,uint32_t& output) {
        if(!inputs || !count || first>=16 || count>16-first) return 60;
        if(tiled_) return 91;
        if(attributes.tiling!=1) return 88;
        if(attributes.aspect || attributes.option || attributes.reserved0 || attributes.reserved1) return 89;
        uint32_t group=0;
        while(group<groups_.size() && groups_[group].occupied) ++group;
        if(group==groups_.size()) return 108;
        for(uint32_t i=0;i<count;++i) if(slots_[first+i]) return 109;
        uint32_t installed=0;
        try {
            for(;installed<count;++installed) {
                uint64_t token=0;
                const int result=RegisterLinearFrame(first+installed,attributes.format,attributes.width,
                    attributes.height,attributes.pitch,inputs[installed].bytes,inputs[installed].length,token);
                if(result) {
                    for(uint32_t i=0;i<installed;++i) slots_[first+i].reset();
                    return result;
                }
            }
        } catch(...) {
            for(uint32_t i=0;i<installed;++i) slots_[first+i].reset();
            throw;
        }
        groups_[group]={true,first,count,true,attributes};output=group;return 0;
    }
    bool LinearGroupAttributes(uint32_t group,CtrVideo::DisplayBufferAttributes& output) const {
        if(group>=groups_.size() || !groups_[group].occupied || !groups_[group].linear) return false;
        output=groups_[group].attributes;return true;
    }
    bool UnregisterHostGroup(uint32_t group) {
        if(group>=groups_.size() || !groups_[group].occupied) return false;
        const auto entry=groups_[group];
        for(uint32_t n=0;n<entry.count;++n) slots_[entry.first+n].reset();
        groups_[group]={}; return true;
    }
    Frame CaptureHostSlot(uint32_t slot) const { return slot<slots_.size() ? slots_[slot] : Frame{}; }
    // Host-owned snapshots only; not PS4 RegisterBuffers or a guest memory lease.
    uint64_t RegisterHostFrame(uint32_t slot, const void* source, size_t bytes, size_t pitch) {
        if (tiled_ || slot >= slots_.size() || slots_[slot] || nextToken_ == UINT64_MAX ||
            !source || !extent_.width || !extent_.height || extent_.width > 16384 || extent_.height > 16384) return 0;
        const size_t row = size_t(extent_.width) * 4;
        if (pitch < row || pitch > SIZE_MAX / extent_.height || bytes < pitch * (extent_.height - 1) + row) return 0;
        auto frame = std::make_shared<HostFrame>();
        frame->pitch = row; frame->bytes.resize(row * extent_.height);
        for (uint32_t y = 0; y < extent_.height; ++y)
            std::memcpy(frame->bytes.data() + size_t(y) * row,
                        static_cast<const unsigned char*>(source) + size_t(y) * pitch, row);
        frame->slot=slot; frame->owner = identity_; frame->token = ++nextToken_; slots_[slot] = frame; return frame->token;
    }
    // Publish a new immutable snapshot under the same registered slot token.
    // Already queued requests retain the previous bytes. Failure leaves the slot intact.
    bool RefreshHostFrame(uint32_t slot, uint64_t token, const void* source, size_t bytes, size_t pitch) {
        if (!CaptureHostFrame(slot, token) || !source) return false;
        const size_t row = size_t(extent_.width) * 4;
        if (pitch < row || pitch > SIZE_MAX / extent_.height || bytes < pitch * (extent_.height - 1) + row) return false;
        auto frame = std::make_shared<HostFrame>();
        frame->slot=slot; frame->pitch = row; frame->token = token; frame->owner = identity_;
        frame->bytes.resize(row * extent_.height);
        for (uint32_t y = 0; y < extent_.height; ++y)
            std::memcpy(frame->bytes.data() + size_t(y) * row,
                        static_cast<const unsigned char*>(source) + size_t(y) * pitch, row);
        slots_[slot] = frame; return true;
    }
    int WriteLinearFrame(uint32_t slot, uint32_t sourceFormat, uint32_t width, uint32_t height,
                            uint32_t pitchPixels, const void* source, size_t length, uint64_t& output, bool refreshing) {
        if(refreshing && !CaptureHostFrame(slot,output)) return 77;
        // Retail/public VideoOut 32bpp sRGB byte layouts only. HDR/tiled conversion missing.
        if (sourceFormat != 0x80000000U && sourceFormat != 0x80002200U) return 88;
        if (width != extent_.width || height != extent_.height || pitchPixels < width || !source) return 89;
        const size_t pitch = size_t(pitchPixels) * 4;
        const size_t row = size_t(width) * 4;
        if (!height || pitch > SIZE_MAX / height || length < pitch * (height - 1) + row) return 89;
        const bool targetBgra = format_ == VK_FORMAT_B8G8R8A8_UNORM || format_ == VK_FORMAT_B8G8R8A8_SRGB;
        if (!targetBgra && format_ != VK_FORMAT_R8G8B8A8_UNORM && format_ != VK_FORMAT_R8G8B8A8_SRGB) return 88;
        std::vector<unsigned char> converted(row * height);
        const bool swap = targetBgra != (sourceFormat == 0x80000000U);
        for (uint32_t y = 0; y < height; ++y) for (uint32_t x = 0; x < width; ++x) {
            const auto* input = static_cast<const unsigned char*>(source) + size_t(y) * pitch + size_t(x) * 4;
            auto* pixel = converted.data() + size_t(y) * row + size_t(x) * 4;
            pixel[0] = input[swap ? 2 : 0]; pixel[1] = input[1]; pixel[2] = input[swap ? 0 : 2]; pixel[3] = input[3];
        }
        if(refreshing) return RefreshHostFrame(slot,output,converted.data(),converted.size(),row) ? 0 : 77;
        const auto token = RegisterHostFrame(slot, converted.data(), converted.size(), row);
        if (!token) return 61; output = token; return 0;
    }
    int RegisterLinearFrame(uint32_t slot,uint32_t format,uint32_t width,uint32_t height,
        uint32_t pitch,const void* source,size_t length,uint64_t& output) {
        return WriteLinearFrame(slot,format,width,height,pitch,source,length,output,false);
    }
    int RefreshLinearFrame(uint32_t slot,uint64_t token,uint32_t format,uint32_t width,uint32_t height,
        uint32_t pitch,const void* source,size_t length) {
        return WriteLinearFrame(slot,format,width,height,pitch,source,length,token,true);
    }
    Frame CaptureHostFrame(uint32_t slot, uint64_t token) const {
        if (slot >= slots_.size() || !slots_[slot] || !token || slots_[slot]->token != token) return {};
        return slots_[slot];
    }
    bool UnregisterHostFrame(uint32_t slot, uint64_t token) {
        if (!CaptureHostFrame(slot, token)) return false;
        for(const auto& group:groups_)
            if(group.occupied && slot>=group.first && slot-group.first<group.count) return false;
        slots_[slot].reset(); return true;
    }
    // A captured frame owns its bytes through deferred consumption, even after unregistration.
    int PresentHostFrame(const Frame& frame) {
        if (!frame || frame->owner != identity_) return 44;
        return Present(frame->bytes.data(), frame->bytes.size(), frame->pitch);
    }
    // Bounded owner-thread FIFO; independent of the kernel event queue.
    bool EnqueueHostFrame(const Frame& frame, uint64_t argument) {
        if (eventError_ || !frame || frame->owner != identity_ || pending_.size() >= 16) return false;
        uint64_t micros=0,tsc=0;
        const int clock=events_.ReadClock(micros,tsc);
        if(clock==2) return false;
        pending_.push_back({frame, argument});
        submitClockValid_=clock==0; submitTsc_=tsc; return true;
    }
    int AdjustIdentityColor(float gamma) {
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        // Identity applies exactly in the existing byte-preserving presentation path.
        // Nonidentity (including NaN/Inf) requires an actual transform, not a successful stub.
        if(gamma!=1.0f)return 89;
        presentationGamma_=gamma;return 0;
    }
    int BindSessionClock(const CtrDisplayEventApi& api) {
        if(GetWindowThreadProcessId(window_,nullptr)!=GetCurrentThreadId())return 116;
        if(completed_ || !pending_.empty())return 91;
        return events_.BindClock(api)?91:0;
    }
    int AttachEvents(const CtrDisplayEventApi& api, uint64_t queue, uint64_t flipData, uint64_t vblankData) {
        if(eventError_ || portToken_ || eventsAttached_) return 91;
        if(events_.BindClock(api)) return 91;
        if(events_.Add(api,queue,6,flipData)) return 91;
        if(events_.Add(api,queue,7,vblankData)) { events_.Remove(queue,6); return 91; }
        combinedQueue_=queue;
        eventsAttached_=true; return 0;
    }
    int DetachEvents() {
        if(portToken_ || !eventsAttached_) return 91;
        eventsAttached_=false;
        const int flip=events_.Remove(combinedQueue_,6), blank=events_.Remove(combinedQueue_,7);
        combinedQueue_=0; return flip || blank ? 91 : 0;
    }
    int AddEvent(const CtrDisplayEventApi& api,uint64_t queue,uint64_t ident,uint64_t data) {
        if(events_.BindClock(api)) return 91;
        return events_.Add(api,queue,ident,data) ? 91 : 0;
    }
    int RemoveEvent(uint64_t queue,uint64_t ident) { return events_.Remove(queue,ident) ? 91 : 0; }
    int VblankStatus(uint64_t& count,uint64_t& micros,uint64_t& tsc) {
        return events_.VblankSnapshot(count,micros,tsc) ? 0 : 91;
    }
    void Progress(uint64_t& completed,uint64_t& token,uint64_t& argument,uint64_t& pending,int64_t& slot) const {
        completed=completed_; token=lastToken_; argument=lastArgument_; pending=pending_.size(); slot=lastSlot_;
    }
    void FlipTiming(uint64_t& submit,uint64_t& micros,uint64_t& tsc,uint32_t& valid) const {
        submit=submitTsc_; micros=completedMicros_; tsc=completedTsc_;
        valid=(submitClockValid_?1U:0U)|(completeClockValid_?2U:0U);
    }
    struct Completion { uint64_t count, token, argument; };
    // Owner-thread transaction: queue admission failure restores prior slot snapshot.
    int SubmitLinearFrame(uint32_t slot,uint64_t token,uint32_t format,uint32_t width,uint32_t height,
        uint32_t pitch,const void* source,size_t length,uint64_t argument) {
        if(eventError_) return eventError_;
        const auto previous=CaptureHostFrame(slot,token);
        if(!previous) return 77;
        if(pending_.size()>=16) return 63;
        const int result=RefreshLinearFrame(slot,token,format,width,height,pitch,source,length);
        if(result) return result;
        try {
            if(EnqueueHostFrame(CaptureHostFrame(slot,token),argument)) return 0;
        } catch(...) {slots_[slot]=previous;throw;}
        slots_[slot]=previous;return 63;
    }
    int PresentNext(Completion& output) {
        if(eventError_) return eventError_;
        if (pending_.empty()) return 47;
        if(completed_==UINT64_MAX) return 98;
        const auto& request = pending_.front();
        const int status = PresentHostFrame(request.frame);
        if (status) return status; // Keep failed request; do not signal successful completion.
        uint64_t completionMicros=0,completionTsc=0;
        const int clock=events_.ReadClock(completionMicros,completionTsc);
        const Completion done{++completed_, request.frame->token, request.argument};
        lastToken_=done.token; lastArgument_=done.argument; lastSlot_=request.frame->slot;
        completeClockValid_=clock==0; completedMicros_=completionMicros; completedTsc_=completionTsc;
        pending_.pop_front(); // GPU completed: never replay this request if event delivery fails.
        if(clock==2) {eventError_=91; return 91;}
        if(events_.Emit(6,done.argument)) { eventError_=91; return 91; }
        output=done; return 0;
    }
    // This session owns its port clock; queued snapshots cannot accidentally use another port.
    bool StartPacing() {
        if (portToken_) return false;
        portToken_ = port_.Open([](void* c) { return static_cast<VideoDisplayEventSet*>(c)->VblankTick(); }, &events_); return portToken_ != 0;
    }
    bool StopPacing() {
        if (!portToken_) return false;
        const auto token = portToken_; portToken_ = 0;
        return port_.Close(token);
    }
    int WaitVblank(std::chrono::milliseconds timeout) {
        if(eventError_) return eventError_;
        uint64_t before = 0;
        int error=0;
        if(!port_.Count(portToken_,before)) return 52;
        const bool tick=port_.WaitAfter(portToken_,before,timeout);
        // A callback failure stops the clock and wakes waiters false. Preserve its
        // cause instead of reporting an ordinary timeout/closed port.
        if(!port_.CallbackError(portToken_,error) || error) { eventError_=91; return 91; }
        return tick ? 0 : 52;
    }
    int PresentNextAtVblank(std::chrono::milliseconds timeout, Completion& output) {
        if (pending_.empty()) return 47;
        const auto deadline=std::chrono::steady_clock::now()+timeout;
        for(;;) {
            const auto now=std::chrono::steady_clock::now();
            if(now>=deadline) return 52;
            auto left=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-now);
            if(left.count()==0) left=std::chrono::milliseconds(1);
            const int status=WaitVblank(left);
            if(status) return status;
            const uint64_t count=events_.VblankCount();
            // Public PresentThread tests the old counter before advancing vblank.
            if(count && (count-1) % (uint64_t(flipRate_)+1)==0) return PresentNext(output);
        }
    }
    int PumpMessages() {
        MSG message{};
        while (PeekMessageW(&message, window_, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        return *closeRequested_ ? 80 : 0;
    }
    int Present(const void* source, size_t bytes, size_t pitch) {
        return present_control(device_, get_, swap_, family_, count_, extent_, format_, memory_, source, bytes, pitch);
    }
};
