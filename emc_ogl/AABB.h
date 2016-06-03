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
