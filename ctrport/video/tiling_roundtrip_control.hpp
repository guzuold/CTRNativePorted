// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "tiling_image_control.hpp"
#include "thin_image_control.hpp"
#include "thin_detile.hpp"
// Synthetic whole public shader GPU roundtrip, including padded1088 rows.
static int ctr_tiling_roundtrip(VkDevice device,PFN_vkGetDeviceProcAddr get,uint32_t family,
    const VkPhysicalDeviceMemoryProperties& memory,VkPipelineLayout layout,const VkPipeline* pipelines,
    VkDescriptorSetLayout descriptorLayout,bool thin=false,uint32_t pitch=1920,uint32_t paddedHeight=1088,std::vector<unsigned char>* tiledOutput=nullptr) {
#define LOAD(name) const auto name=reinterpret_cast<PFN_##name>(get(device,#name));if(!name)return 91
    LOAD(vkCreateBuffer);LOAD(vkDestroyBuffer);LOAD(vkGetBufferMemoryRequirements);LOAD(vkAllocateMemory);
    LOAD(vkFreeMemory);LOAD(vkBindBufferMemory);LOAD(vkMapMemory);LOAD(vkUnmapMemory);
    LOAD(vkCreateDescriptorPool);LOAD(vkDestroyDescriptorPool);LOAD(vkAllocateDescriptorSets);LOAD(vkUpdateDescriptorSets);
    LOAD(vkCreateCommandPool);LOAD(vkDestroyCommandPool);LOAD(vkAllocateCommandBuffers);
    LOAD(vkBeginCommandBuffer);LOAD(vkEndCommandBuffer);LOAD(vkCmdPipelineBarrier);LOAD(vkCmdBindPipeline);
    LOAD(vkCmdBindDescriptorSets);LOAD(vkCmdDispatch);LOAD(vkGetDeviceQueue);LOAD(vkQueueSubmit);
    LOAD(vkCreateFence);LOAD(vkDestroyFence);LOAD(vkWaitForFences);LOAD(vkQueueWaitIdle);
#undef LOAD
    if(!pitch || !paddedHeight || pitch%128 || paddedHeight%64 || uint64_t(pitch)*paddedHeight/64>65535)return 88;
    const uint32_t texels=pitch*paddedHeight;const VkDeviceSize bytes=VkDeviceSize(texels)*4;
    std::vector<unsigned char> captured(tiledOutput?static_cast<size_t>(bytes):0);
    VkBuffer buffers[4]{};VkDeviceMemory allocations[4]{};void* mapped[4]{};
    VkDescriptorPool descriptors{};VkCommandPool commands{};VkFence fence{};VkQueue queue{};bool submitted=false;
    vkGetDeviceQueue(device,family,0,&queue);if(!queue)return 91;
    const auto run=[&]() -> int {
        for(uint32_t i=0;i<4;++i) {
            const VkDeviceSize size=i==3 ? 268 : bytes+256;
            VkBufferCreateInfo bi{};bi.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;bi.size=size;
            bi.usage=i==3 ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
            if(vkCreateBuffer(device,&bi,nullptr,&buffers[i])!=VK_SUCCESS)return 91;
            VkMemoryRequirements requirements{};vkGetBufferMemoryRequirements(device,buffers[i],&requirements);
            uint32_t type=UINT32_MAX;
            constexpr VkMemoryPropertyFlags flags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            for(uint32_t j=0;j<memory.memoryTypeCount;++j)if((requirements.memoryTypeBits&(1U<<j)) &&
                (memory.memoryTypes[j].propertyFlags&flags)==flags) {type=j;break;}
            if(type==UINT32_MAX)return 88;
            VkMemoryAllocateInfo ai{};ai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            ai.allocationSize=requirements.size;ai.memoryTypeIndex=type;
            if(vkAllocateMemory(device,&ai,nullptr,&allocations[i])!=VK_SUCCESS ||
               vkBindBufferMemory(device,buffers[i],allocations[i],0)!=VK_SUCCESS ||
               vkMapMemory(device,allocations[i],0,size,0,&mapped[i])!=VK_SUCCESS)return 91;
        }
        auto* input=static_cast<uint32_t*>(mapped[0]);
        for(uint32_t i=0;i<texels;++i)input[i]=(i*2654435761U)^((i/pitch)*0x9e37U)^0xa5c37129U;
        std::memset(mapped[1],0x39,static_cast<size_t>(bytes));std::memset(mapped[2],0x57,static_cast<size_t>(bytes));
        for(uint32_t i=0;i<3;++i)std::memset(static_cast<unsigned char*>(mapped[i])+bytes,0xd3,256);
        auto* uniform=static_cast<uint32_t*>(mapped[3]);std::memset(uniform,0,268);
        uniform[0]=0;uniform[1]=1;uniform[2]=1; // bank_swizzle,slices,mips
        uniform[3]=static_cast<uint32_t>(bytes);uniform[4]=pitch;uniform[5]=paddedHeight;uniform[6]=0;
        const VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,4},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2}};
        VkDescriptorPoolCreateInfo pi{};pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pi.maxSets=2;pi.poolSizeCount=2;pi.pPoolSizes=sizes;
        if(vkCreateDescriptorPool(device,&pi,nullptr,&descriptors)!=VK_SUCCESS)return 91;
        const VkDescriptorSetLayout layouts[]={descriptorLayout,descriptorLayout};VkDescriptorSet sets[2]{};
        VkDescriptorSetAllocateInfo di{};di.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        di.descriptorPool=descriptors;di.descriptorSetCount=2;di.pSetLayouts=layouts;
        if(vkAllocateDescriptorSets(device,&di,sets)!=VK_SUCCESS)return 91;
        VkDescriptorBufferInfo infos[6]{};VkWriteDescriptorSet writes[6]{};
        // Both shaders binding0=tiled, binding1=linear; direction is compiled.
        const uint32_t indices[]={1,0,3,1,2,3};
        for(uint32_t i=0;i<6;++i) {
            infos[i]={buffers[indices[i]],0,i%3==2 ? 268 : bytes};
            writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=sets[i/3];
            writes[i].dstBinding=i%3;writes[i].descriptorCount=1;writes[i].pBufferInfo=&infos[i];
            writes[i].descriptorType=i%3==2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        vkUpdateDescriptorSets(device,6,writes,0,nullptr);
        VkCommandPoolCreateInfo ci{};ci.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;ci.queueFamilyIndex=family;
        if(vkCreateCommandPool(device,&ci,nullptr,&commands)!=VK_SUCCESS)return 91;
        VkCommandBufferAllocateInfo ca{};ca.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ca.commandPool=commands;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
        VkCommandBuffer command{};if(vkAllocateCommandBuffers(device,&ca,&command)!=VK_SUCCESS)return 91;
        VkCommandBufferBeginInfo begin{};begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if(vkBeginCommandBuffer(command,&begin)!=VK_SUCCESS)return 91;
        VkMemoryBarrier barrier{};barrier.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        for(uint32_t i=0;i<2;++i) {
            vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipelines[i]);
            vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,layout,0,1,&sets[i],0,nullptr);
            vkCmdDispatch(command,texels/64,1,1); // Exact domain; whole upstream shader has no tail bound check.
            barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask=i==0 ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                i==0 ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
        }
        if(vkEndCommandBuffer(command)!=VK_SUCCESS)return 91;
        VkFenceCreateInfo fi{};fi.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if(vkCreateFence(device,&fi,nullptr,&fence)!=VK_SUCCESS)return 91;
        VkSubmitInfo si{};si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;si.commandBufferCount=1;si.pCommandBuffers=&command;
        if(vkQueueSubmit(queue,1,&si,fence)!=VK_SUCCESS)return 91;submitted=true;
        const auto waited=vkWaitForFences(device,1,&fence,VK_TRUE,5000000000ULL);
        if(waited!=VK_SUCCESS)return waited==VK_TIMEOUT ? 52 : 91;
        // Guarded backing exceeds the descriptor range, exposing accidental writes.
        for(uint32_t i=0;i<3;++i)for(size_t j=0;j<256;++j)
            if(static_cast<unsigned char*>(mapped[i])[static_cast<size_t>(bytes)+j]!=0xd3)return 151;
        for(uint32_t i=0;i<texels;++i)
            if(input[i]!=((i*2654435761U)^((i/pitch)*0x9e37U)^0xa5c37129U))return 151;
        for(uint32_t i=0;i<67;++i) {
            const uint32_t expected=i==1 || i==2 ? 1U : i==3 ? static_cast<uint32_t>(bytes) : i==4 ? pitch : i==5 ? paddedHeight : 0U;
            if(uniform[i]!=expected)return 151;
        }
        if(std::memcmp(mapped[0],mapped[2],static_cast<size_t>(bytes)))return 147;
        if(!std::memcmp(mapped[0],mapped[1],static_cast<size_t>(bytes)))return 148;
        std::printf("whole public tiling GPU roundtrip pitch=%u padded_height=%u bytes=%llu exact=true tiled_differs=true fence=true guards768=true input_uniform_immutable=true synthetic=true retail_pixels0=true R3=false\n",pitch,paddedHeight,static_cast<unsigned long long>(bytes));
        if(tiledOutput) {
            std::memcpy(captured.data(),mapped[1],static_cast<size_t>(bytes));
            tiledOutput->swap(captured);return 0;
        }
        if(thin) {
            std::puts("whole public Thin2DThin32 tile14 GPU inverse roundtrip with guards; synthetic only");
            std::vector<unsigned char> converted{0x71,0x93};
            if(ctr_thin_detile(device,get,family,memory,layout,pipelines[1],descriptorLayout,
                pitch,paddedHeight,mapped[1],bytes-1,converted)!=60 || converted!=std::vector<unsigned char>({0x71,0x93}))return 151;
            for(const auto invalidPitch:{127U,4097U}) {
                if(ctr_thin_detile(device,get,family,memory,layout,pipelines[1],descriptorLayout,
                    invalidPitch,1088,mapped[1],bytes,converted)!=88 || converted!=std::vector<unsigned char>({0x71,0x93}))return 151;
            }
            if(ctr_thin_detile(device,get,family,memory,layout,pipelines[1],descriptorLayout,
                1920,63,mapped[1],bytes,converted)!=88 || converted!=std::vector<unsigned char>({0x71,0x93}))return 151;
            if(ctr_thin_detile(device,get,family,memory,layout,pipelines[1],descriptorLayout,
                4096,4096,mapped[1],67108864,converted)!=88 || converted!=std::vector<unsigned char>({0x71,0x93}))return 151;
            const int detiled=ctr_thin_detile(device,get,family,memory,layout,pipelines[1],descriptorLayout,
                pitch,paddedHeight,mapped[1],bytes,converted);
            if(detiled)return detiled;
            if(converted.size()!=bytes || std::memcmp(converted.data(),mapped[0],static_cast<size_t>(bytes)))return 147;
            std::printf("bounded owned Thin2DThin extent pitch=%u padded_height=%u bytes=%llu exact=true guards=true negatives_atomic=true\n",
                pitch,paddedHeight,static_cast<unsigned long long>(bytes));
            if(pitch!=1920 || paddedHeight!=1088)return 0;
            return ctr_thin_image_control(device,get,family,memory,converted.data(),mapped[0]);
        }
        return ctr_tiling_image_control(device,get,family,memory,buffers[2],buffers[1],mapped[0],mapped[1]);
    };
    const int result=run();if(submitted && result)vkQueueWaitIdle(queue);
    if(fence)vkDestroyFence(device,fence,nullptr);if(commands)vkDestroyCommandPool(device,commands,nullptr);
    if(descriptors)vkDestroyDescriptorPool(device,descriptors,nullptr);
    for(uint32_t i=0;i<4;++i) {
        if(mapped[i])vkUnmapMemory(device,allocations[i]);
        if(buffers[i])vkDestroyBuffer(device,buffers[i],nullptr);
        if(allocations[i])vkFreeMemory(device,allocations[i],nullptr);
    }
    return result;
}
