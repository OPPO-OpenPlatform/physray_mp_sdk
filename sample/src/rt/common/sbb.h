/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#pragma once

#include <ph/va.h>

class SceneBuildBuffers : public ph::va::DeferredHostOperation {
    std::list<ph::va::BufferObject>    _buffers;
    ph::va::VulkanSubmissionProxy &    _vsp;
    std::vector<std::function<void()>> _deferredJobs;
    bool                               _finished = false;

public:
    SceneBuildBuffers(ph::va::SimpleVulkanDevice & dev): DeferredHostOperation(dev.vgi()), _vsp(dev.graphicsQ()) {};

    ~SceneBuildBuffers() { finish(); }

    void finish() {
        PH_REQUIRE(!_finished);
        _finished = true;

        // run all deferred job
        for (auto & f : _deferredJobs) f();
        _deferredJobs.clear();
    };

    void deferUntilGPUWorkIsDone(std::function<void()> func) override {
        PH_REQUIRE(!_finished);
        PH_REQUIRE(func);
        _deferredJobs.push_back(std::move(func));
    }

    template<typename T, VkBufferUsageFlags USAGE = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT>
    ph::va::BufferObject * allocatePermanentBuffer(ph::ConstRange<T> data, const char * name = nullptr) {
        PH_REQUIRE(!_finished);

        if (!name || !*name) name = "<unnamed>";

        uint32_t size = (uint32_t) (data.size() * sizeof(T));

        // copy data to scratch buffer
        auto scratch = ph::va::BufferObject(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ph::va::DeviceMemoryUsage::CPU_ONLY, 0);
        scratch.allocate(_vsp.vgi(), size, "scratch buffer");
        {
            const auto & mapped = scratch.map<uint8_t>();
            memcpy(mapped.range.data(), data.data(), size);
        }

        // allocate permanent buffer
        auto & permanent = _buffers.emplace_front(USAGE);
        permanent.allocate(_vsp.vgi(), size, name);

        // copy data from scratch buffer to permanent buffer
        ph::va::SingleUseCommandPool pool(_vsp);
        pool.syncexec([&](auto cb) {
            auto region      = VkBufferCopy {};
            region.srcOffset = 0;
            region.dstOffset = 0;
            region.size      = size;
            vkCmdCopyBuffer(cb, scratch.buffer, permanent.buffer, 1, &region);
        });
        PH_LOGI("Upload %s to GPU buffer: handle=0x%llu", name, permanent.buffer);
        return &permanent;
    }
};