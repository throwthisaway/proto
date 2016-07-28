#include "Helpers.h"
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 GetModel(const glm::mat4& m, const glm::vec3& pos, float rot, const glm::vec3& pivot, float scale) {
	return glm::translate(
		glm::rotate(
			glm::translate(glm::scale(glm::translate(m, pos), glm::vec3{ scale, scale, 1.f }), pivot), rot, { 0.f, 0.f, 1.f }), -pivot);
}
namespace {
	inline auto Transf2D(float x, float y, const glm::mat4& m) {
		return glm::vec3(m * glm::vec4{ x, y, 0.f, 1.f });
	}
}
void AABBToBBoxEdgesCCW(const AABB& aabb, const glm::mat4& m, std::vector<glm::vec3>& res) {
	res.push_back(Transf2D(aabb.l, aabb.b, m));
	const auto first = res.back();
	res.push_back(Transf2D(aabb.r, aabb.b, m));
	res.push_back(res.back());
	res.push_back(Transf2D(aabb.r, aabb.t, m));
	res.push_back(res.back());
	res.push_back(Transf2D(aabb.l, aabb.t, m));
	res.push_back(res.back());
	res.push_back(first);
}

void AABBToBBoxEdgesCCW(const AABB& aabb, std::vector<glm::vec3>& res) {
	res.push_back({ aabb.l, aabb.b, 0.f });
	const auto first = res.back();
	res.push_back({ aabb.r, aabb.b, 0.f });
	res.push_back(res.back());
	res.push_back({ aabb.r, aabb.t, 0.f });
	res.push_back(res.back());
	res.push_back({ aabb.l, aabb.t, 0.f });
	res.push_back(res.back());
	res.push_back(first);
}


AABB TransformAABB(const AABB& aabb, const glm::mat4& m) {
	const auto v1 = Transf2D(aabb.l, aabb.b, m),
		v2 = Transf2D(aabb.r, aabb.b, m),
		v3 = Transf2D(aabb.r, aabb.t, m),
		v4 = Transf2D(aabb.l, aabb.t, m);
	return{ std::min(v1.x, std::min(v2.x, std::min(v3.x, v4.x))),
		std::max(v1.y, std::max(v2.y, std::max(v3.y, v4.y))),
		std::max(v1.x, std::max(v2.x, std::max(v3.x, v4.x))),
		std::min(v1.y, std::min(v2.y, std::min(v3.y, v4.y))) };
}

BBox TransformBBox(const AABB& aabb, const glm::mat4& m) {
	BBox res;
	res[0] = Transf2D(aabb.l, aabb.b, m);
	res[1] = Transf2D(aabb.r, aabb.b, m);
	res[2] = Transf2D(aabb.r, aabb.t, m);
	res[3] = Transf2D(aabb.l, aabb.t, m);
	return res;
}
