#include "Helpers.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include "Globals.h"

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

OBB TransformBBox(const AABB& aabb, const glm::mat4& m) {
	OBB res;
	res[0] = Transf2D(aabb.l, aabb.b, m);
	res[1] = Transf2D(aabb.r, aabb.b, m);
	res[2] = Transf2D(aabb.r, aabb.t, m);
	res[3] = Transf2D(aabb.l, aabb.t, m);
	return res;
}

std::vector<glm::vec3> MergeOBBs(const OBB& obb1, const OBB& obb2) {
	std::vector<glm::vec3> res(obb1.size() + obb2.size());
	auto out = std::copy(std::begin(obb1), std::end(obb1), std::begin(res));
	std::copy(std::begin(obb2), std::end(obb2), out);
	return res;
}
namespace {
	// Lexicographical less-than
	inline bool lessVec3(const glm::vec3& lhs, const glm::vec3& rhs) {
		if (lhs.x < rhs.x)
			return true;
		if (lhs.x == rhs.x && lhs.y < rhs.y)
			return true;
		if (lhs.x == rhs.x && lhs.y == rhs.y && lhs.z < rhs.z)
			return true;
		return false;
	}
	// CCW - < 0, CW - > 0
	inline float cross2D(const glm::vec3& p, const glm::vec3& q) {
		return p.x*q.y - p.y*q.x;
	}
	inline float dir(const glm::vec3& p, const glm::vec3& q, const glm::vec3& r) {
		return cross2D(p - q, r - q);
	}
}

std::vector<glm::vec3> ConvexHullCCW(std::vector<glm::vec3> points) {
	std::vector<glm::vec3> res;
	std::sort(std::begin(points), std::end(points), lessVec3);
	for (const auto& p : points) {
		while (res.size() >= 2 && dir(res[res.size() - 2], res[res.size() - 1], p) >= 0)  res.pop_back();
		res.push_back(p);
	}
	//auto start = res.size();
	//for (const auto& p : points) {
	//	while (res.size() >= start && dir(res[res.size() - 2], res[res.size() - 1], p) <= 0)  res.pop_back();
	//	res.push_back(p);
	//}
	for (size_t i = points.size() - 2, start = res.size() + 1; i< points.size(); --i) {
		while (res.size() >= start && dir(res[res.size() - 2], res[res.size() - 1], points[i]) >= 0)  res.pop_back();
		res.push_back(points[i]);
	}
	return res;
}

std::vector<glm::vec3> GetConvexHullOfOBBSweep(const OBB& obb, const OBB& prev_obb) {
	return ConvexHullCCW(MergeOBBs(obb, prev_obb));
}

bool Cull(const glm::vec3& pos, const glm::mat4& vp) {
	const auto ndc = vp * glm::vec4(pos, 1.f);
	return ndc.x < -CULL || ndc.y < -CULL || ndc.x > CULL && ndc.y > CULL;
}
