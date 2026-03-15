#include "frame_graph.h"
#include <nlohmann/json.hpp>

namespace flare::renderer {

	VkFormat FrameGraph::toVkFormat(const std::string& format) {
		static const std::unordered_map<std::string, VkFormat> table = {
			{"VK_FORMAT_R8_UNORM",            VK_FORMAT_R8_UNORM},
			{"VK_FORMAT_R8G8_UNORM",          VK_FORMAT_R8G8_UNORM},
			{"VK_FORMAT_R8G8B8A8_UNORM",      VK_FORMAT_R8G8B8A8_UNORM},
			{"VK_FORMAT_B8G8R8A8_UNORM",      VK_FORMAT_B8G8R8A8_UNORM},
			{"VK_FORMAT_R16_SFLOAT",          VK_FORMAT_R16_SFLOAT},
			{"VK_FORMAT_R16G16_SFLOAT",       VK_FORMAT_R16G16_SFLOAT},
			{"VK_FORMAT_R16G16B16A16_SFLOAT", VK_FORMAT_R16G16B16A16_SFLOAT},
			{"VK_FORMAT_R32_SFLOAT",          VK_FORMAT_R32_SFLOAT},
			{"VK_FORMAT_R32G32_SFLOAT",       VK_FORMAT_R32G32_SFLOAT},
			{"VK_FORMAT_R32G32B32A32_SFLOAT", VK_FORMAT_R32G32B32A32_SFLOAT},
			{"VK_FORMAT_D16_UNORM",           VK_FORMAT_D16_UNORM},
			{"VK_FORMAT_D24_UNORM_S8_UINT",   VK_FORMAT_D24_UNORM_S8_UINT},
			{"VK_FORMAT_D32_SFLOAT",          VK_FORMAT_D32_SFLOAT},
			{"VK_FORMAT_D32_SFLOAT_S8_UINT",  VK_FORMAT_D32_SFLOAT_S8_UINT},
		};
		auto it = table.find(format);
		if (it != table.end()) return it->second;
		std::fprintf(stderr, "[FrameGraph] WARN: unknown format '%s', use VK_FORMAT_UNDEFINED.\n", format.c_str());
		return VK_FORMAT_UNDEFINED;
	}

	VkAttachmentLoadOp FrameGraph::toVkLoadOp(const std::string& op) {
		static const std::unordered_map<std::string, VkAttachmentLoadOp> map = {
			{"VK_ATTACHMENT_LOAD_OP_LOAD",       VK_ATTACHMENT_LOAD_OP_LOAD},
			{"VK_ATTACHMENT_LOAD_OP_CLEAR",      VK_ATTACHMENT_LOAD_OP_CLEAR},
			{"VK_ATTACHMENT_LOAD_OP_DONT_CARE",  VK_ATTACHMENT_LOAD_OP_DONT_CARE}
		};
		if (auto it = map.find(op); it != map.end())
			return it->second;
		return VK_ATTACHMENT_LOAD_OP_LOAD;
	}

	FrameGraphResourceType FrameGraph::toResourceType(const std::string& type) {
		if (type == "Buffer" )     return FrameGraphResourceType::Buffer;
		if (type == "Attachment")  return FrameGraphResourceType::Attachment;
		if (type == "Texture" )    return FrameGraphResourceType::Texture;
		if (type == "Reference")   return FrameGraphResourceType::Reference;
		std::fprintf(stderr, "[FrameGraph] WARN: unknown type '%s', default to Texture.\n", type.c_str());
	    return FrameGraphResourceType::Texture;
	}

	bool FrameGraph::isDepthFormat(VkFormat format) {
		switch (format) {
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return true;
		default:
			return false;
		}
	}

	bool FrameGraph::loadFromJsonFile(const std::string& path) {

		mNodes.clear();
		mResourceMap.clear();
		mAdjacencyList.clear();
		mTopologyOrder.clear();

		// 1 open json
		std::ifstream ifs(path);
		if (!ifs.is_open()) {
			std::fprintf(stderr, "[FrameGraph] error: fail to load frame graph json file");
			return false;
		}
		nlohmann::json jGraph; ifs >> jGraph;

		// 2 assign node
		for (const auto& jNode : jGraph["nodes"]) {
			FrameGraphNode node{};
			node.name = jNode.value("name", "");
			// 2.1 input
			if (jNode.contains("inputs") && jNode["inputs"].is_array()) {
				for (const auto& jInput : jNode["inputs"]) {
					std::string name = jInput.value("resource", "");
					FrameGraphResourceType type = toResourceType(jInput.value("access", "Reference"));
					node.inputs.push_back(name);
					
					auto it = mResourceMap.find(name);
					if (it == mResourceMap.end()) {
						FrameGraphResource resource{};
						resource.name = name;
						resource.type = type;
						mResourceMap.emplace(name, std::move(resource));
					}
				}
			}
			// 2.2 output
			if (jNode.contains("outputs") && jNode["outputs"].is_array()) {
				for (const auto& jOutput : jNode["outputs"]) {
					std::string name = jOutput.value("resource", "");
					FrameGraphResourceType type = toResourceType(jOutput.value("access", "Reference"));
					node.outputs.push_back(name);
					
					auto it = mResourceMap.find(name);
					if (it == mResourceMap.end()) {
						FrameGraphResource resource{};
						resource.name = name;
						resource.type = type;
						if (type == FrameGraphResourceType::Attachment) {
							resource.attachment.format = toVkFormat(jOutput.value("format", "VK_FORMAT_UNDEFINED"));
							resource.attachment.width = static_cast<uint32_t>(jOutput["resolution"][0].get<int>());
							resource.attachment.height = static_cast<uint32_t>(jOutput["resolution"][1].get<int>());
							resource.attachment.loadOp = toVkLoadOp(jOutput.value("op", "VK_ATTACHMENT_LOAD_OP_LOAD"));
						}
						mResourceMap.emplace(name, std::move(resource));
					}
				}
			}
			mNodes.emplace_back(std::move(node));
		}
		return true;
	}

	bool FrameGraph::buildDependency(std::string& error) {
		mAdjacencyList.assign(static_cast<uint32_t>(mNodes.size()), {});
		
		// 1 number producer
		for (uint32_t i = 0; i < mNodes.size(); ++i) {
			const auto& node = mNodes[i];
			for (const auto& out : node.outputs) {
				auto it = mResourceMap.find(out);
				if (it == mResourceMap.end()) {
					error = "Output resource '" + out + "' not found in resource table.";
					return false; 
				}
				auto& res = it->second;
				res.producer = i;
			}
		}
		// 2 mark external
		for (auto& [name, res] : mResourceMap) {
			res.attachment.external = (res.producer == std::numeric_limits<uint32_t>::max());
		}
		// 2 create adjacency list
		for (uint32_t i = 0; i < mNodes.size(); ++i) {
			const auto& node = mNodes[i];
			for (const auto& in : node.inputs) {
				auto it = mResourceMap.find(in);
				if (it == mResourceMap.end()) continue;
				auto& producerRes = it->second;
				if (producerRes.attachment.external) continue;
				if (producerRes.producer != i) {
					mAdjacencyList[producerRes.producer].push_back(i);
					producerRes.refCount++;
				}
			}
		}

		return true;
	}

	bool FrameGraph::buildTopology(std::string& error) {
		const uint32_t n = static_cast<uint32_t>(mNodes.size());
		mTopologyOrder.clear();
		mTopologyOrder.reserve(n);

		// 1 dependency count
		std::vector<uint32_t> indeg(n, 0);
		for (uint32_t i = 0; i < n; ++i) {
			for (auto v : mAdjacencyList[i]) indeg[v]++;
		}
		// 2 Kahn'a algorithm
		std::queue<uint32_t> q;
		for (uint32_t i = 0; i < n; ++i) {
			if (indeg[i] == 0) q.push(i);
		}
		while (!q.empty()) {
			auto u = q.front(); q.pop();
			mTopologyOrder.push_back(u);
			for (auto v : mAdjacencyList[u]) {
				if (--indeg[v] == 0) q.push(v);
			}
		}
		// 3 cyclic check
		if (mTopologyOrder.size() != n) {
			error = "Cyclic dependency detected in frame graph.";
			return false;
		}
		return true;
	}

	void FrameGraph::createImages() {
		for (auto& [name, res] : mResourceMap) {
			if (res.type != FrameGraphResourceType::Attachment) continue;
			auto& info = res.attachment;
			if (info.external || info.image) continue;

			// ==== exception: hi z ====
			if (name == "hzb") {
				auto calcMipCount = [](uint32_t w, uint32_t h) {
					uint32_t m = 1;
					while (w > 1 || h > 1) {
						w = std::max(1u, w / 2);
						h = std::max(1u, h / 2);
						++m;
					}
					return m;
				};

				const VkImageUsageFlags usage =VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				const VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
				const uint32_t mipLevels = calcMipCount(info.width, info.height);

				info.image = Image::create(mDevice, info.width, info.height, info.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage,
										 VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, aspect, 0, 1, VK_IMAGE_VIEW_TYPE_2D, mipLevels);
				continue; 
			}

			// ==== exception: storage image ====
			if (name == "motion_vectors" || name == "visibility" || name == "indirect_lighting") {
				const VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
				const VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

				info.image = Image::create(mDevice, info.width, info.height, info.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
								usage, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, aspect);
				continue;
			}

			// ==== other attachments ====
			const bool isDepth = isDepthFormat(info.format);
			VkImageUsageFlags usage = isDepth 
				? (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
				: (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
					VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

			VkImageAspectFlags aspect = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			info.image = Image::create(mDevice, info.width, info.height, info.format, VK_IMAGE_TYPE_2D,
						 VK_IMAGE_TILING_OPTIMAL, usage, VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, aspect);
		}
	}

	bool FrameGraph::setRecord(const std::string & passName, std::function<void()> record) {
		for (auto& n : mNodes) {
			if (n.name == passName) {
				n.record = std::move(record);
				return true;
			}
		}
		std::fprintf(stderr, "[FrameGraph] ERROR: setRecord failed, node '%s' not found.\n", passName.c_str());
		return false;
	}

	bool FrameGraph::compile() {
		// 1 dependency 
		std::string error;
		if (!buildDependency(error)) {
			std::fprintf(stderr, "[FrameGraph] ERROR: %s\n", error.c_str());
			return false;
		}
		// 2 topology
		if (!buildTopology(error)) {
			std::fprintf(stderr, "[FrameGraph] ERROR: %s\n", error.c_str());
			return false;
		}
		return true;
	}

	Image::Ptr FrameGraph::getImage(const std::string& name) const {
		auto it = mResourceMap.find(name);
		if (it == mResourceMap.end()) return nullptr;
		if (it->second.type != FrameGraphResourceType::Attachment) return nullptr;
		return it->second.attachment.image;
	}

	VkFormat FrameGraph::getAttachmentFormat(const std::string& name) const {
		auto it = mResourceMap.find(name);
		if (it == mResourceMap.end()) return VK_FORMAT_UNDEFINED;
		if (it->second.type != FrameGraphResourceType::Attachment) return VK_FORMAT_UNDEFINED;
		return it->second.attachment.format;
	}

	std::pair<uint32_t, uint32_t> FrameGraph::getAttachmentSize(const std::string& name) const {
		auto it = mResourceMap.find(name);
		if (it == mResourceMap.end()) return { 0,0 };
		if (it->second.type != FrameGraphResourceType::Attachment) return { 0,0 };
		return { it->second.attachment.width, it->second.attachment.height };
	}

	void FrameGraph::execute() {
		if (!mTopologyOrder.empty()) {
			for (uint32_t idx : mTopologyOrder) {
				auto& n = mNodes[idx];
				if (n.record) n.record();
			}
		} else {
			for (auto& n : mNodes) if (n.record) n.record();
		}
	}

	void FrameGraph::clear() {
		mAdjacencyList.clear();
		mTopologyOrder.clear();
		mResourceMap.clear();
		mNodes.clear();
	}
}


