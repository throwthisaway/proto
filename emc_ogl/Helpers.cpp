#include "Helpers.h"
#include <glm/gtc/matrix_transform.hpp>
#include "AABB.h"

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