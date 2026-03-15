#pragma once
#include "../../common.h"
#include "../../platform/graphics/device.h"
#include "../../platform/graphics/command_pool.h"
#include "../../platform/graphics/image.h"
#include "../../platform/graphics/render_pass.h"
#include "../../platform/graphics/framebuffer.h"
#include "gpu_resource/texture/texture.h"

namespace flare::renderer {
	using namespace flare::vk;

	enum class FrameGraphResourceType {
		Buffer,
		Attachment,
		Texture,
		Reference
	};

	struct FrameGraphAttachmentInfo {
		VkFormat            format = VK_FORMAT_UNDEFINED;
		uint32_t            width = 0;
		uint32_t            height = 0;
		uint32_t            layers = 1;
		VkAttachmentLoadOp  loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		bool                external = false;
		Image::Ptr          image{nullptr};
	};

	struct FrameGraphResource {
		std::string              name;
		FrameGraphResourceType   type = FrameGraphResourceType::Reference;
		FrameGraphAttachmentInfo attachment{};
		uint32_t                 producer = std::numeric_limits<uint32_t>::max();
		uint32_t                 refCount = 0;                             
	};

	struct FrameGraphNode {
		std::string name;
		std::vector<std::string> inputs;
		std::vector<std::string> outputs;
		std::function<void()> record;
	};

	class FrameGraph {
	public:
		FrameGraph() = default;
		~FrameGraph() = default;

		void init(Device::Ptr device, CommandPool::Ptr commandPool) { mDevice = device; mCommandPool = commandPool; }
		bool loadFromJsonFile(const std::string& path);
		void createImages();
		bool setRecord(const std::string& passName, std::function<void()> record);
		bool compile();
		void execute();
		void clear();

	public:
		VkFormat getAttachmentFormat(const std::string& name) const;
		std::pair<uint32_t, uint32_t> getAttachmentSize(const std::string& name) const;
		Image::Ptr getImage(const std::string& name) const;

	private:
		bool buildDependency(std::string& error);
		bool buildTopology(std::string& error);

		// tools
		static bool isDepthFormat(VkFormat format);
		static VkFormat toVkFormat(const std::string& format);
		static VkAttachmentLoadOp toVkLoadOp(const std::string& op);
		static FrameGraphResourceType toResourceType(const std::string& type);

	private:
		std::vector<FrameGraphNode> mNodes;
		std::unordered_map<std::string, FrameGraphResource> mResourceMap;
		std::vector<std::vector<uint32_t>> mAdjacencyList;
		std::vector<uint32_t> mTopologyOrder;

		Device::Ptr mDevice{ nullptr };
		CommandPool::Ptr mCommandPool{ nullptr };
	};
}