#pragma once

#include "../../common.h"
#include "device.h"
#include "description.h"

namespace flare::vk {

	class SubPass {
	public:
		SubPass();
		~SubPass();

		void addColorAttachmentReference(const VkAttachmentReference& ref);
		void addInputAttachmentReference(const VkAttachmentReference& ref);
		void setDepthStencilAttachmentReference(const VkAttachmentReference& ref);
		void setResolveAttachmentReference(const VkAttachmentReference& ref);
		void setDepthResolveAttachmentReference(const VkAttachmentReference& ref);
		void buildSubPassDescription();

		[[nodiscard]] auto getSubPassDescription() const { return mSubPassDescription; }


	private:

		VkSubpassDescription mSubPassDescription{};
		std::vector<VkAttachmentReference> mColorAttachmentReferences{};
		std::vector<VkAttachmentReference> mInputAttachmentReferences{};
		VkAttachmentReference mDepthStencilAttachmentReference{};
		VkAttachmentReference mResolvedAttachmentReference{};
		VkAttachmentReference mDepthResolvedAttachmentReference{};
	};

	class RenderPass {
	public:

		using Ptr = std::shared_ptr<RenderPass>;
		static Ptr create(const Device::Ptr& device) { return std::make_shared<RenderPass>(device); }

		RenderPass(const Device::Ptr& device);
		~RenderPass();

		void addSubPass(const SubPass& subpass);
		void addDependency(const VkSubpassDependency& dependency);
		void addAttachment(const VkAttachmentDescription& attachmentDes);
		
		void addResolveAttachmentDesc(const AttachmentDesc& desc);
		void addColorAttachmentDesc(const AttachmentDesc& desc);
		void addDepthResolveAttachmentDesc(const AttachmentDesc& desc);
		void addDepthAttachmentDesc(const AttachmentDesc& desc);

		void buildRenderPass();

		[[nodiscard]] auto getRenderPass() const { return mRenderPass; }
		[[nodiscard]] auto getAttachmentCount() const { return mAttachmentDescriptions.size(); }


		[[nodiscard]] auto& getResolveAttachmentDesc() const { return mResolveAttachmentDescs; }
		[[nodiscard]] auto& getColorAttachmentDesc() const { return mColorAttachmentDescs; }
		[[nodiscard]] auto& getResolveADepthAttachmentDesc() const { return mDepthResolveAttachmentDesc; }
		[[nodiscard]] auto& getDepthAttachmentDesc() const { return mDepthAttachmentDesc; }

	private:

		Device::Ptr mDevice{ nullptr };
		VkRenderPass mRenderPass{ VK_NULL_HANDLE };

		std::vector<SubPass> mSubPasses{};
		std::vector<VkSubpassDependency> mDependencies{};
		std::vector<VkAttachmentDescription> mAttachmentDescriptions{};
		
		std::vector<AttachmentDesc> mResolveAttachmentDescs{};
		std::vector<AttachmentDesc> mColorAttachmentDescs{};
		std::optional<AttachmentDesc> mDepthResolveAttachmentDesc{};
		std::optional<AttachmentDesc> mDepthAttachmentDesc{};
	};
}