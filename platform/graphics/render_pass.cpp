#include "render_pass.h"

namespace flare::vk {

	SubPass::SubPass(){}

	SubPass::~SubPass(){}

	void SubPass::addColorAttachmentReference(const VkAttachmentReference& ref){
	
		mColorAttachmentReferences.push_back(ref);
	}

	void SubPass::addInputAttachmentReference(const VkAttachmentReference& ref){
	
		mInputAttachmentReferences.push_back(ref);
	}

	void SubPass::setDepthStencilAttachmentReference(const VkAttachmentReference& ref){
	
		mDepthStencilAttachmentReference = ref;
	}

	void SubPass::setResolveAttachmentReference(const VkAttachmentReference& ref) {
		
		mResolvedAttachmentReference = ref;
	}

	void SubPass::buildSubPassDescription() {
		
		if (mColorAttachmentReferences.empty()) {
			throw std::runtime_error("Error: color attachment group is empty!");
		}

		mSubPassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		mSubPassDescription.colorAttachmentCount = static_cast<uint32_t>(mColorAttachmentReferences.size());
		mSubPassDescription.pColorAttachments = mColorAttachmentReferences.data();
		mSubPassDescription.inputAttachmentCount = static_cast<uint32_t>(mInputAttachmentReferences.size());
		mSubPassDescription.pInputAttachments = mInputAttachmentReferences.data();

		//
		if (mResolvedAttachmentReference.layout != VK_IMAGE_LAYOUT_UNDEFINED) {
			mSubPassDescription.pResolveAttachments = &mResolvedAttachmentReference;
		}
		else {
			mSubPassDescription.pResolveAttachments = nullptr;
		}

		// 
		if (mDepthStencilAttachmentReference.layout != VK_IMAGE_LAYOUT_UNDEFINED) {
			mSubPassDescription.pDepthStencilAttachment = &mDepthStencilAttachmentReference;
		}
		else {
			mSubPassDescription.pDepthStencilAttachment = nullptr;
		}
	}

	RenderPass::RenderPass(const Device::Ptr& device) {
		mDevice = device;
	}

	RenderPass::~RenderPass() {
		if (mRenderPass != VK_NULL_HANDLE) {
			vkDestroyRenderPass(mDevice->getDevice(), mRenderPass, nullptr);
		}
	}

	void RenderPass::addSubPass(const SubPass& subpass) { mSubPasses.push_back(subpass); }

	void RenderPass::addDependency(const VkSubpassDependency& dependency) { mDependencies.push_back(dependency); }

	void RenderPass::addAttachment(const VkAttachmentDescription& attachmentDes) { mAttachmentDescriptions.push_back(attachmentDes); }

	void RenderPass::addColorAttachmentDesc(const AttachmentDesc& desc) {
		mColorAttachmentDescs.push_back(desc);
	}

	void RenderPass::addResolveAttachmentDesc(const AttachmentDesc& desc) {
		
		mResolveAttachmentDescs.push_back(desc);
	}

	void RenderPass::addDepthAttachmentDesc(const AttachmentDesc& desc) {
		mDepthAttachmentDesc = desc;
	}

	void RenderPass::addDepthResolveAttachmentDesc(const AttachmentDesc& desc) {
		mDepthResolveAttachmentDesc = desc;
	}

	void RenderPass::buildRenderPass() {

		if (mSubPasses.empty() || mAttachmentDescriptions.empty() || mDependencies.empty()) {
			throw std::runtime_error("Error: not enough elements to build renderPass");
		}

		// 1 Unwrap subpass
		std::vector<VkSubpassDescription> subPasses{};
		for (int i = 0; i < mSubPasses.size(); ++i) {
			subPasses.push_back(mSubPasses[i].getSubPassDescription());
		}

		// 2 Fill in create info
		VkRenderPassCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

		createInfo.attachmentCount = static_cast<uint32_t>(mAttachmentDescriptions.size());
		createInfo.pAttachments = mAttachmentDescriptions.data();

		createInfo.dependencyCount = static_cast<uint32_t>(mDependencies.size());
		createInfo.pDependencies = mDependencies.data();

		createInfo.subpassCount = static_cast<uint32_t>(subPasses.size());
		createInfo.pSubpasses = subPasses.data();

		// 3 Create render pass
		if (vkCreateRenderPass(mDevice->getDevice(), &createInfo, nullptr, &mRenderPass) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create renderPass");
		}
	}
}