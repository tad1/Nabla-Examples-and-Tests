#include <nabla.h>

#include "nbl/video/utilities/CSimpleResizeSurface.h"
#include "nbl/application_templates/MonoAssetManagerAndBuiltinResourceApplication.hpp"
#include "../common/SimpleWindowedApplication.hpp"

#include "app_resources/common.hlsl"

using namespace nbl;
using namespace core;
using namespace ui;
using namespace system;
using namespace asset;
using namespace video;

constexpr int DURATION = 1000;



class EmptyEventCallback : public ISimpleManagedSurface::ICallback {
public:
	EmptyEventCallback(nbl::system::logger_opt_smart_ptr&& logger) : m_logger(std::move(logger)) {}
	EmptyEventCallback() {}

	void setLogger(nbl::system::logger_opt_smart_ptr& logger)
	{
		m_logger = logger;
	}
private:

	void onMouseConnected_impl(nbl::core::smart_refctd_ptr<nbl::ui::IMouseEventChannel>&& mch) override
	{
		m_logger.log("A mouse %p has been connected", nbl::system::ILogger::ELL_INFO, mch.get());
	}
	void onMouseDisconnected_impl(nbl::ui::IMouseEventChannel* mch) override
	{
		m_logger.log("A mouse %p has been disconnected", nbl::system::ILogger::ELL_INFO, mch);
	}
	void onKeyboardConnected_impl(nbl::core::smart_refctd_ptr<nbl::ui::IKeyboardEventChannel>&& kbch) override
	{
		m_logger.log("A keyboard %p has been connected", nbl::system::ILogger::ELL_INFO, kbch.get());
	}
	void onKeyboardDisconnected_impl(nbl::ui::IKeyboardEventChannel* kbch) override
	{
		m_logger.log("A keyboard %p has been disconnected", nbl::system::ILogger::ELL_INFO, kbch);
	}

private:
	nbl::system::logger_opt_smart_ptr m_logger = nullptr;
};
class HelloWorldInNabla : public examples::SimpleWindowedApplication, public application_templates::MonoAssetManagerAndBuiltinResourceApplication {
	using device_base_t = examples::SimpleWindowedApplication;
	using asset_base_t = application_templates::MonoAssetManagerAndBuiltinResourceApplication;

	_NBL_STATIC_INLINE_CONSTEXPR uint32_t WIN_W = 600, WIN_H = 400, SC_IMG_COUNT = 3u, FRAMES_IN_FLIGHT = 5u;

public:

	inline HelloWorldInNabla(const path& _localInputCWD, const path& _localOutputCWD, const path& _sharedInputCWD, const path& _sharedOutputCWD)
		: IApplicationFramework(_localInputCWD, _localOutputCWD, _sharedInputCWD, _sharedOutputCWD) {};


	// Inherited via MonoSystemMonoLoggerApplication
	void workLoopBody() override
	{

		const auto resourceIx = m_submitIx % m_maxFramesInFlight;

		if (m_submitIx >= m_maxFramesInFlight)
		{
			const ISemaphore::SWaitInfo cmdbufDonePending[] = {
				{
					.semaphore = m_semaphore.get(),
					.value = m_submitIx + 1 - m_maxFramesInFlight
				}
			};
			if (m_device->blockForSemaphores(cmdbufDonePending) != ISemaphore::WAIT_RESULT::SUCCESS)
				return;
		}

		if (!m_cmdPools[resourceIx]->reset())
			return;
		auto cmdbuf = m_cmdBufs[resourceIx].get();

		// Acquire
		auto acquire = m_surface->acquireNextImage();
		if (!acquire)
			return;

		{
			cmdbuf->begin(IGPUCommandBuffer::USAGE::ONE_TIME_SUBMIT_BIT);

			{
				const IGPUCommandBuffer::SClearColorValue colorValue = { .float32 = {1.f,0.f,1.f,1.f} };
				const IGPUCommandBuffer::SClearDepthStencilValue depthValue = { .depth = 0.f };
				auto fb = static_cast<CDefaultSwapchainFramebuffers*>(m_surface->getSwapchainResources())->getFramebuffer(acquire.imageIndex);
				const IGPUCommandBuffer::SRenderpassBeginInfo info = {
					.framebuffer = fb,
					.colorClearValues = &colorValue,
					.depthStencilClearValues = &depthValue,
					.renderArea = {
						.offset = {0,0},
						.extent = {fb->getCreationParameters().width,fb->getCreationParameters().height}
					}
				};
				cmdbuf->beginRenderPass(info, IGPUCommandBuffer::SUBPASS_CONTENTS::INLINE);
			}
			cmdbuf->bindGraphicsPipeline(m_pipeline.get());
			{
				auto fb = static_cast<CDefaultSwapchainFramebuffers*>(m_surface->getSwapchainResources())->getFramebuffer(acquire.imageIndex);
				const VkRect2D currentRenderArea =
				{
					.offset = {0,0},
					.extent = {fb->getCreationParameters().width,fb->getCreationParameters().height}
				};
				// set viewport
				{
					const asset::SViewport viewport =
					{
						.width = float(fb->getCreationParameters().width),
						.height = float(fb->getCreationParameters().height)
					};
					cmdbuf->setViewport({ &viewport,1 });
				}
				cmdbuf->setScissor({ &currentRenderArea,1 });
			}
			//{
			PushConstants constants;
			constants.phase = sinf((float)(m_submitIx % DURATION) * 2 / (float)DURATION);
			cmdbuf->pushConstants(m_pipeline->getLayout(), IGPUShader::ESS_FRAGMENT, 0, sizeof(PushConstants), &constants);
			//cmdbuf->bindDescriptorSets(nbl::asset::EPBP_GRAPHICS, m_pipeline->getLayout(), 3, 1, &m_gpuDescriptorSet.get());
			cmdbuf->draw(3, 1, 0, 0);
			//}
			//

			cmdbuf->endRenderPass();
			cmdbuf->end();
		}

		const IQueue::SSubmitInfo::SSemaphoreInfo rendered[1] = { {
			.semaphore = m_semaphore.get(),
			.value = ++m_submitIx,
			.stageMask = PIPELINE_STAGE_FLAGS::COLOR_ATTACHMENT_OUTPUT_BIT
		} };

		auto queue = getGraphicsQueue();
		queue->startCapture();
		// submit
		{
			{
				const IQueue::SSubmitInfo::SCommandBufferInfo commandBuffers[1] = { {
					.cmdbuf = cmdbuf
				} };
				// we don't need to wait for the transfer semaphore, because we submit everything to the same queue
				const IQueue::SSubmitInfo::SSemaphoreInfo acquired[1] = { {
					.semaphore = acquire.semaphore,
					.value = acquire.acquireCount,
					.stageMask = PIPELINE_STAGE_FLAGS::NONE
				} };
				const IQueue::SSubmitInfo infos[1] = { {
					.waitSemaphores = acquired,
					.commandBuffers = commandBuffers,
					.signalSemaphores = rendered
				} };
				// we won't signal the sema if no success
				if (queue->submit(infos) != IQueue::RESULT::SUCCESS)
					m_submitIx--;
			}
		}

		// Present
		m_surface->present(acquire.imageIndex, rendered);
		queue->endCapture();
	}

	bool keepRunning() override
	{
		return dynamic_cast<ISimpleManagedSurface::ICallback*>(m_window->getEventCallback())->isWindowOpen() && !m_surface->irrecoverable();
	}


	// Inherited via SimpleWindowedApplication
	core::vector<video::SPhysicalDeviceFilter::SurfaceCompatibility> getSurfaces() const override
	{
		if (!m_surface) {
			{
				auto windowCallback = core::make_smart_refctd_ptr<EmptyEventCallback>(smart_refctd_ptr(m_logger));
				IWindow::SCreationParams params = {};
				params.callback = core::smart_refctd_ptr<nbl::video::ISimpleManagedSurface::ICallback>();
				params.width = WIN_W;
				params.height = WIN_H;
				params.x = 32;
				params.y = 32;
				params.flags = IWindow::ECF_BORDERLESS | IWindow::ECF_HIDDEN;
				params.windowCaption = "Hello World! in Nabla";
				params.callback = windowCallback;
				const_cast<std::remove_const_t<decltype (m_window)>&>(m_window) = m_winMgr->createWindow(std::move(params));
			}
			auto surface = CSurfaceVulkanWin32::create(smart_refctd_ptr(m_api), smart_refctd_ptr_static_cast<IWindowWin32>(m_window));
			const_cast<std::remove_const_t<decltype (m_surface)>&>(m_surface) = nbl::video::CSimpleResizeSurface<CDefaultSwapchainFramebuffers>::create(std::move(surface));

		}
		if (m_surface) {
			return { {m_surface->getSurface()} };
		}
		return {};
	}

	smart_refctd_ptr<ICPUShader> compileCPUShader(const std::string& shaderPath) {
		IAssetLoader::SAssetLoadParams lp = {};
		lp.logger = m_logger.get();
		lp.workingDirectory = ""; //virtual root?

		SAssetBundle assetBundle = m_assetMgr->getAsset(shaderPath, lp);
		const auto assets = assetBundle.getContents();
		if (assets.empty()) {
			logFail("Could not load shader!");
			assert(0);
		}

		assert(assets.size() == 1);
		smart_refctd_ptr<ICPUShader> source = IAsset::castDown<ICPUShader>(assets[0]);
		smart_refctd_ptr<const CSPIRVIntrospector::CStageIntrospectionData> introspection;
		{
			auto* compilerSet = m_assetMgr->getCompilerSet();
			nbl::asset::IShaderCompiler::SCompilerOptions options = {};
			options.stage = source->getStage(); //TODO: handle generic shaders
			options.targetSpirvVersion = m_device->getPhysicalDevice()->getLimits().spirvVersion;
			options.spirvOptimizer = nullptr;
			options.debugInfoFlags |= IShaderCompiler::E_DEBUG_INFO_FLAGS::EDIF_SOURCE_BIT;
			options.preprocessorOptions.sourceIdentifier = source->getFilepathHint();
			options.preprocessorOptions.logger = m_logger.get();
			options.preprocessorOptions.includeFinder = compilerSet->getShaderCompiler(source->getContentType())->getDefaultIncludeFinder();

			auto spirvUnspecialized = compilerSet->compileToSPIRV(source.get(), options);

			{
				auto* srcContent = spirvUnspecialized->getContent();
				system::ISystem::future_t<core::smart_refctd_ptr<system::IFile>> future;
				m_physicalDevice->getSystem()->createFile(future, system::path("../app_resources/compiled.spv"), system::IFileBase::ECF_WRITE);
				if (auto file = future.acquire(); file && bool(*file)) {
					system::IFile::success_t success;
					(*file)->write(success, srcContent->getPointer(), 0, srcContent->getSize());
					success.getBytesProcessed(true);
				}
			}
			source = std::move(spirvUnspecialized);
		}
		return source;
	}

	inline bool onAppInitialized(smart_refctd_ptr<ISystem>&& system) override {
		if (!device_base_t::onAppInitialized(smart_refctd_ptr(system)))
			return false;
		if (!asset_base_t::onAppInitialized(std::move(system)))
			return false;


		//{
		//	smart_refctd_ptr<nbl::asset::IShaderCompiler> compiler = make_smart_refctd_ptr<nbl::asset::CHLSLCompiler>(smart_refctd_ptr(m_system));
		//	constexpr const char* fragmentSource = R"===(
		//				#pragma wave shader_stage(compute)

		//				[[vk::binding(0,0)]] RWStructuredBuffer<uint32_t> buff;

		//				[numthreads(WORKGROUP_SIZE,1,1)]
		//				void main(uint32_t3 ID : SV_DispatchThreadID)
		//				{
		//					buff[ID.x] = ID.x;
		//				}
		//			)===";
		//	const auto orientationAsUint32 = static_cast<uint32_t>(hlsl::SurfaceTransform::FLAG_BITS::IDENTITY_BIT);

		//	IGPUShader::SSpecInfo::spec_constant_map_t specConstants;
		//	specConstants[0] = { .data = &orientationAsUint32,.size = sizeof(orientationAsUint32) };
		//	ICPUShader fragCode("", );
		//	auto fragmentShader = m_device->createShader()
		//	const IGPUShader::SSpecInfo fragSpec = {
		//			.entryPoint = "main",
		//			.shader = fragmentShader.get()
		//	};

		//	const IGPUShader::SSpecInfo shaders[2] = {
		//		{.shader = m_vxShader.get(), .entries = &specConstants},

		//	};
		//	IGPUGraphicsPipeline::SCreationParams params[1];
		//	//params[0].layout = layout;
		//	//params[0].shaders = shaders;
		//	params[0].cached = {
		//		.vertexInput = {},
		//		.primitiveAssembly = {},
		//		.rasterization = {
		//			.faceCullingMode = asset::EFCM_NONE,
		//			.depthWriteEnable = false
		//		},
		//		.blend = {},
		//		.subpassIx = 0
		//	};
		//	m_device->createGraphicsPipelines(nullptr, params, &m_pipeline);
		//}

		ISwapchain::SCreationParams swapchainParams = { .surface = smart_refctd_ptr<ISurface>(m_surface->getSurface()) };
		if (!swapchainParams.deduceFormat(m_physicalDevice))
			return logFail("Could not choose a Surface Format for the Swapchain!");
		auto cpuFragShader = this->compileCPUShader("app_resources/triangle.frag");
		smart_refctd_ptr<nbl::video::IGPUShader> fragShader = m_device->createShader(cpuFragShader.get());
		if (!fragShader)
			return logFail("Failed to create a GPU Fragment Shader, seems the Driver doesn't like the SPIR-V we're feeding it!\n");

		auto cpuVertexShader = this->compileCPUShader("app_resources/triangle.vert");
		smart_refctd_ptr<nbl::video::IGPUShader> vertexShader = m_device->createShader(cpuVertexShader.get());
		if (!vertexShader)
			return logFail("Failed to create a GPU Vertex Shader, seems the Driver doesn't like the SPIR-V we're feeding it!\n");

		const static IGPURenderpass::SCreationParams::SSubpassDependency dependencies[] = {
			// wipe-transition of Color to ATTACHMENT_OPTIMAL
			{
				.srcSubpass = IGPURenderpass::SCreationParams::SSubpassDependency::External,
				.dstSubpass = 0,
				.memoryBarrier = {
				// last place where the depth can get modified in previous frame
				.srcStageMask = PIPELINE_STAGE_FLAGS::LATE_FRAGMENT_TESTS_BIT,
				// only write ops, reads can't be made available
				.srcAccessMask = ACCESS_FLAGS::DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				// destination needs to wait as early as possible
				.dstStageMask = PIPELINE_STAGE_FLAGS::EARLY_FRAGMENT_TESTS_BIT,
				// because of depth test needing a read and a write
				.dstAccessMask = ACCESS_FLAGS::DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | ACCESS_FLAGS::DEPTH_STENCIL_ATTACHMENT_READ_BIT
			}
			// leave view offsets and flags default
		},
			// color from ATTACHMENT_OPTIMAL to PRESENT_SRC
			{
				.srcSubpass = 0,
				.dstSubpass = IGPURenderpass::SCreationParams::SSubpassDependency::External,
				.memoryBarrier = {
				// last place where the depth can get modified
				.srcStageMask = PIPELINE_STAGE_FLAGS::COLOR_ATTACHMENT_OUTPUT_BIT,
				// only write ops, reads can't be made available
				.srcAccessMask = ACCESS_FLAGS::COLOR_ATTACHMENT_WRITE_BIT
				// spec says nothing is needed when presentation is the destination
			}
			// leave view offsets and flags default
		},
		IGPURenderpass::SCreationParams::DependenciesEnd
		};

		auto scResources = std::make_unique<CDefaultSwapchainFramebuffers>(m_device.get(), swapchainParams.surfaceFormat.format, dependencies);
		auto* renderpass = scResources->getRenderpass();

		if(!renderpass)
			return logFail("Failed to create Renderpass!");

		m_semaphore = m_device->createSemaphore(m_submitIx);
		if (!m_semaphore)
			return logFail("Failed to Create a Semaphore!");

		auto gfxQueue = getGraphicsQueue();
		if (!m_surface || !m_surface->init(gfxQueue, std::move(scResources), swapchainParams.sharedParams))
			return logFail("Could not create Window & Surface or initialize the Surface!");
		m_maxFramesInFlight = m_surface->getMaxFramesInFlight();
		//m_logger->log("m_maxFramesInFlight = %d", ILogger::ELL_DEBUG, m_maxFramesInFlight);
		

		// create the commandbuffers
		for (auto i = 0u; i < m_maxFramesInFlight; i++)
		{
			m_cmdPools[i] = m_device->createCommandPool(gfxQueue->getFamilyIndex(), IGPUCommandPool::CREATE_FLAGS::NONE);
			if (!m_cmdPools[i])
				return logFail("Couldn't create Command Pool!");
			if (!m_cmdPools[i]->createCommandBuffers(IGPUCommandPool::BUFFER_LEVEL::PRIMARY, { m_cmdBufs.data() + i,1 }))
				return logFail("Couldn't create Command Buffer!");
		}

		m_winMgr->setWindowSize(m_window.get(), WIN_W, WIN_H);
		m_surface->recreateSwapchain();

		SPushConstantRange pushConstantRanges[] = {
			{
			.stageFlags = IShader::ESS_VERTEX,
			.offset = 0,
			.size = sizeof(PushConstants)
			}
		};

		nbl::video::IGPUDescriptorSetLayout::SBinding bindings[] = {
			{
				.binding = 0u,
				.type = asset::IDescriptor::E_TYPE::ET_UNIFORM_BUFFER,
				.createFlags = IGPUDescriptorSetLayout::SBinding::E_CREATE_FLAGS::ECF_NONE,
				.stageFlags = asset::IShader::ESS_VERTEX | asset::IShader::ESS_FRAGMENT,
				.count = 1u,
			}
		};

		auto descriptorSetLayout = m_device->createDescriptorSetLayout(bindings);
		{
			const video::IGPUDescriptorSetLayout* const layouts[] = { nullptr, descriptorSetLayout.get() };
			const uint32_t setCounts[] = { 0u, 1u };
			m_descriptorPool = m_device->createDescriptorPoolForDSLayouts(IDescriptorPool::E_CREATE_FLAGS::ECF_NONE, layouts, setCounts);
			if (!m_descriptorPool)
				return logFail("Failed to Create Descriptor Pool");
		}
		m_gpuDescriptorSet = m_descriptorPool->createDescriptorSet(descriptorSetLayout);

		if(!m_gpuDescriptorSet)
			return logFail("Could not create Descriptor Set!");

		auto pipelineLayout = m_device->createPipelineLayout(pushConstantRanges, nullptr, std::move(descriptorSetLayout));

		if(!pipelineLayout)
			return logFail("Could not create Pipeline Layout!");

		const IGPUShader::SSpecInfo fragSpec = {
			.entryPoint = "main",
			.shader = fragShader.get()
		};

		const IGPUShader::SSpecInfo vertexSpec = {
			.entryPoint = "main",
			.shader = vertexShader.get()

		};
		//create pipeline
		//(fragSpec,layout.get(),scResources->getRenderpass()
		{
			auto layout = pipelineLayout.get();
			const uint32_t subpassIx = 0;
			SBlendParams blendParams = {};
			const hlsl::SurfaceTransform::FLAG_BITS swapchainTransform = hlsl::SurfaceTransform::FLAG_BITS::IDENTITY_BIT;
			

			if (!renderpass || hlsl::bitCount(swapchainTransform) != 1)
				m_pipeline = nullptr;

			using namespace ::nbl::video;
			auto device = const_cast<ILogicalDevice*>(renderpass->getOriginDevice());

			core::smart_refctd_ptr<IGPUGraphicsPipeline> m_retval;
			{
				const auto orientationAsUint32 = static_cast<uint32_t>(swapchainTransform);

				IGPUShader::SSpecInfo::spec_constant_map_t specConstants;
				specConstants[0] = { .data = &orientationAsUint32,.size = sizeof(orientationAsUint32) };

				const IGPUShader::SSpecInfo shaders[2] = {
					vertexSpec,
					fragSpec
				};

				IGPUGraphicsPipeline::SCreationParams params[1];
				params[0].layout = layout;
				params[0].shaders = shaders;
				params[0].cached = {
					.vertexInput = {}, // The Full Screen Triangle doesn't use any HW vertex input state
					.primitiveAssembly = {},
					.rasterization = {
						.faceCullingMode = asset::EFCM_NONE,
						.depthWriteEnable = false
					},
					.blend = blendParams,
					.subpassIx = subpassIx
				};
				params[0].renderpass = renderpass;

				if (!device->createGraphicsPipelines(nullptr, params, &m_retval))
					m_pipeline = nullptr;
			}
			m_pipeline = m_retval;
		}


		m_winMgr->show(m_window.get());
		return true;
		
	}
private:
	smart_refctd_ptr<IWindow> m_window;
	smart_refctd_ptr<CSimpleResizeSurface<CDefaultSwapchainFramebuffers>> m_surface;
	smart_refctd_ptr<IGPUGraphicsPipeline> m_pipeline;
	smart_refctd_ptr<ISemaphore> m_semaphore;

	uint64_t m_submitIx : 59 = 0;
	uint64_t m_maxFramesInFlight : 5;

	const PushConstants pc = { .phase = 0.f };

	std::array<smart_refctd_ptr<IGPUCommandPool>, ISwapchain::MaxImages> m_cmdPools;
	std::array<smart_refctd_ptr<IGPUCommandBuffer>, ISwapchain::MaxImages> m_cmdBufs;

	core::smart_refctd_ptr<video::IDescriptorPool> m_descriptorPool;
	core::smart_refctd_ptr<video::IGPUDescriptorSet> m_gpuDescriptorSet;
};

NBL_MAIN_FUNC(HelloWorldInNabla)