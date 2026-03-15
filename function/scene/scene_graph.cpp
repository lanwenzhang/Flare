#include "scene_graph.h"

namespace flare::scene {

	int addNode(Scene& scene, int parent, int level) {
		const int node = static_cast<int>(scene.hierarchy.size());
		scene.localTransform.push_back(glm::mat4(1.0f));
		scene.globalTransform.push_back(glm::mat4(1.0f));
		Hierarchy h;
		h.parent = parent;
		h.level = level;
		scene.hierarchy.push_back(h);

		if (parent > -1) {
			auto& parentNode = scene.hierarchy[parent];
			if (parentNode.firstChild == -1) {
				parentNode.firstChild = node;
				scene.hierarchy[node].lastSibling = node;
			}
			else {
				int last = scene.hierarchy[parentNode.firstChild].lastSibling;
				scene.hierarchy[last].nextSibling = node;
				scene.hierarchy[parentNode.firstChild].lastSibling = node;
			}
		}
		return node;
	}

	glm::mat4 toMat4(const aiMatrix4x4& a) {
		return glm::mat4(
			a.a1, a.b1, a.c1, a.d1,
			a.a2, a.b2, a.c2, a.d2,
			a.a3, a.b3, a.c3, a.d3,
			a.a4, a.b4, a.c4, a.d4
		);
	}

	bool recalculateGlobalTransforms(Scene& scene) {
		if (scene.hierarchy.empty()) return false;

		scene.globalTransform[0] = scene.localTransform[0];
		for (size_t i = 1; i < scene.hierarchy.size(); ++i) {
			const int parent = scene.hierarchy[i].parent;
			if (parent >= 0) {
				scene.globalTransform[i] = scene.globalTransform[parent] * scene.localTransform[i];
			}
			else {
				scene.globalTransform[i] = scene.localTransform[i];
			}
		}
		return true;
	}

}

