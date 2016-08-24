#pragma once
#include <glm/glm.hpp>
#include <algorithm>

struct AABB {
	float l, t, r, b;
	//AABB(const AABB&) = default;
	AABB Translate(const glm::vec3& pos) const {
		return{ l + pos.x, t + pos.y, r + pos.x, b + pos.y };
	}
	AABB Scale(float s) {
		return{ l * s, t * s, r * s, b * s };
	}
};
inline AABB Union(const AABB& l, const AABB& r) {
	return{ std::min(l.l, r.l), std::max(l.t, r.t), std::max(l.r, r.r), std::min(l.b, r.b) };
}
inline AABB Intersect(const AABB& l, const AABB& r) {
	return{ std::max(l.l, r.l), std::min(l.t, r.t), std::min(l.r, r.r), std::max(l.b, r.b) };
}
inline glm::vec3 Center(const AABB& aabb) {
	return{ (aabb.l + aabb.r) * .5f, (aabb.t + aabb.b) * .5f, 0.f };
}