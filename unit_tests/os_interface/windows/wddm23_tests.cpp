/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/os_interface/windows/gdi_interface.h"
#include "unit_tests/fixtures/gmm_environment_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/mocks/mock_wddm_interface23.h"
#include "unit_tests/os_interface/windows/gdi_dll_fixture.h"
#include "runtime/os_interface/windows/os_context_win.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "test.h"

using namespace OCLRT;

struct Wddm23TestsWithoutWddmInit : public ::testing::Test, GdiDllFixture, public GmmEnvironmentFixture {
    void SetUp() override {
        GmmEnvironmentFixture::SetUp();
        GdiDllFixture::SetUp();

        wddm = static_cast<WddmMock *>(Wddm::createWddm());
        osInterface = std::make_unique<OSInterface>();
        osInterface->get()->setWddm(wddm);

        wddm->featureTable->ftrWddmHwQueues = true;
        wddmMockInterface = new WddmMockInterface23(*wddm);
        wddm->wddmInterface.reset(wddmMockInterface);
        wddm->registryReader.reset(new RegistryReaderMock());
    }

    void init() {
        EXPECT_TRUE(wddm->init());
        osContext = std::make_unique<OsContext>(osInterface.get());
        osContextWin = osContext->get();
    }

    void TearDown() override {
        GdiDllFixture::TearDown();
        GmmEnvironmentFixture::TearDown();
    }

    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<OsContext> osContext;
    OsContextWin *osContextWin = nullptr;
    WddmMock *wddm = nullptr;
    WddmMockInterface23 *wddmMockInterface = nullptr;
};

struct Wddm23Tests : public Wddm23TestsWithoutWddmInit {
    using Wddm23TestsWithoutWddmInit::TearDown;
    void SetUp() override {
        Wddm23TestsWithoutWddmInit::SetUp();
        init();
    }
};

TEST_F(Wddm23Tests, whenCreateContextIsCalledThenEnableHwQueues) {
    EXPECT_TRUE(wddm->wddmInterface->hwQueuesSupported());
    EXPECT_EQ(1u, getCreateContextDataFcn()->Flags.HwQueueSupported);
}

TEST_F(Wddm23Tests, whenCreateHwQueueIsCalledThenSetAllRequiredFieldsAndMonitoredFence) {
    EXPECT_EQ(osContextWin->getContext(), getCreateHwQueueDataFcn()->hHwContext);
    EXPECT_EQ(0u, getCreateHwQueueDataFcn()->PrivateDriverDataSize);
    EXPECT_EQ(nullptr, getCreateHwQueueDataFcn()->pPrivateDriverData);

    EXPECT_TRUE(nullptr != osContextWin->getMonitoredFence().cpuAddress);
    EXPECT_EQ(1u, osContextWin->getMonitoredFence().currentFenceValue);
    EXPECT_NE(static_cast<D3DKMT_HANDLE>(0), osContextWin->getMonitoredFence().fenceHandle);
    EXPECT_NE(static_cast<D3DGPU_VIRTUAL_ADDRESS>(0), osContextWin->getMonitoredFence().gpuAddress);
    EXPECT_EQ(0u, osContextWin->getMonitoredFence().lastSubmittedFence);
}

TEST_F(Wddm23Tests, givenPreemptionModeWhenCreateHwQueueCalledThenSetGpuTimeoutIfEnabled) {
    wddm->setPreemptionMode(PreemptionMode::Disabled);
    wddm->wddmInterface->createHwQueue(wddm->preemptionMode, *osContextWin);
    EXPECT_EQ(0u, getCreateHwQueueDataFcn()->Flags.DisableGpuTimeout);

    wddm->setPreemptionMode(PreemptionMode::MidBatch);
    wddm->wddmInterface->createHwQueue(wddm->preemptionMode, *osContextWin);
    EXPECT_EQ(1u, getCreateHwQueueDataFcn()->Flags.DisableGpuTimeout);
}

TEST_F(Wddm23Tests, whenDestroyHwQueueCalledThenPassExistingHandle) {
    D3DKMT_HANDLE hwQueue = 123;
    osContextWin->setHwQueue(hwQueue);
    wddmMockInterface->destroyHwQueue(osContextWin->getHwQueue());
    EXPECT_EQ(hwQueue, getDestroyHwQueueDataFcn()->hHwQueue);

    hwQueue = 0;
    osContextWin->setHwQueue(hwQueue);
    wddmMockInterface->destroyHwQueue(osContextWin->getHwQueue());
    EXPECT_NE(hwQueue, getDestroyHwQueueDataFcn()->hHwQueue); // gdi not called when 0
}

TEST_F(Wddm23Tests, whenObjectIsDestructedThenDestroyHwQueue) {
    D3DKMT_HANDLE hwQueue = 123;
    osContextWin->setHwQueue(hwQueue);
    osContext.reset();
    EXPECT_EQ(hwQueue, getDestroyHwQueueDataFcn()->hHwQueue);
}

TEST_F(Wddm23Tests, givenCmdBufferWhenSubmitCalledThenSetAllRequiredFiledsAndUpdateMonitoredFence) {
    uint64_t cmdBufferAddress = 123;
    size_t cmdSize = 456;
    auto hwQueue = osContextWin->getHwQueue();
    COMMAND_BUFFER_HEADER cmdBufferHeader = {};

    EXPECT_EQ(1u, osContextWin->getMonitoredFence().currentFenceValue);
    EXPECT_EQ(0u, osContextWin->getMonitoredFence().lastSubmittedFence);

    wddm->submit(cmdBufferAddress, cmdSize, &cmdBufferHeader, *osContextWin);

    EXPECT_EQ(cmdBufferAddress, getSubmitCommandToHwQueueDataFcn()->CommandBuffer);
    EXPECT_EQ(static_cast<UINT>(cmdSize), getSubmitCommandToHwQueueDataFcn()->CommandLength);
    EXPECT_EQ(hwQueue, getSubmitCommandToHwQueueDataFcn()->hHwQueue);
    EXPECT_EQ(osContextWin->getMonitoredFence().fenceHandle, getSubmitCommandToHwQueueDataFcn()->HwQueueProgressFenceId);
    EXPECT_EQ(&cmdBufferHeader, getSubmitCommandToHwQueueDataFcn()->pPrivateDriverData);
    EXPECT_EQ(static_cast<UINT>(sizeof(COMMAND_BUFFER_HEADER)), getSubmitCommandToHwQueueDataFcn()->PrivateDriverDataSize);

    EXPECT_EQ(osContextWin->getMonitoredFence().gpuAddress, cmdBufferHeader.MonitorFenceVA);
    EXPECT_EQ(osContextWin->getMonitoredFence().lastSubmittedFence, cmdBufferHeader.MonitorFenceValue);
    EXPECT_EQ(2u, osContextWin->getMonitoredFence().currentFenceValue);
    EXPECT_EQ(1u, osContextWin->getMonitoredFence().lastSubmittedFence);
}

TEST_F(Wddm23Tests, givenCurrentPendingFenceValueGreaterThanPendingFenceValueWhenSubmitCalledThenCallWaitOnGpu) {
    uint64_t cmdBufferAddress = 123;
    size_t cmdSize = 456;
    COMMAND_BUFFER_HEADER cmdBufferHeader = {};

    *wddm->pagingFenceAddress = 1;
    wddm->currentPagingFenceValue = 1;
    wddm->submit(cmdBufferAddress, cmdSize, &cmdBufferHeader, *osContextWin);
    EXPECT_EQ(0u, wddm->waitOnGPUResult.called);

    wddm->currentPagingFenceValue = 2;
    wddm->submit(cmdBufferAddress, cmdSize, &cmdBufferHeader, *osContextWin);
    EXPECT_EQ(1u, wddm->waitOnGPUResult.called);
}

TEST_F(Wddm23TestsWithoutWddmInit, whenInitCalledThenInitializeNewGdiDDIsAndCallToCreateHwQueue) {
    EXPECT_EQ(nullptr, wddm->gdi->createHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->gdi->destroyHwQueue.mFunc);
    EXPECT_EQ(nullptr, wddm->gdi->submitCommandToHwQueue.mFunc);

    init();
    EXPECT_EQ(1u, wddmMockInterface->createHwQueueCalled);

    EXPECT_NE(nullptr, wddm->gdi->createHwQueue.mFunc);
    EXPECT_NE(nullptr, wddm->gdi->destroyHwQueue.mFunc);
    EXPECT_NE(nullptr, wddm->gdi->submitCommandToHwQueue.mFunc);
}

TEST_F(Wddm23TestsWithoutWddmInit, whenCreateHwQueueFailedThenReturnFalseFromInit) {
    wddmMockInterface->forceCreateHwQueueFail = true;
    init();
    EXPECT_FALSE(osContextWin->isInitialized());
}

TEST_F(Wddm23TestsWithoutWddmInit, givenFailureOnGdiInitializationWhenCreatingHwQueueThenReturnFailure) {
    struct MyMockGdi : public Gdi {
        bool setupHwQueueProcAddresses() override {
            return false;
        }
    };
    auto myMockGdi = new MyMockGdi();
    wddm->gdi.reset(myMockGdi);
    init();
    EXPECT_FALSE(osContextWin->isInitialized());
    EXPECT_EQ(1u, wddmMockInterface->createHwQueueCalled);
    EXPECT_FALSE(wddmMockInterface->createHwQueueResult);
}
