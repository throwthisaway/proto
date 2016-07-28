#pragma once
#include <vector>
#include <array>
#include <glm/glm.hpp>
#include "AABB.h"

inline auto RoundToPowerOf2(unsigned int v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return ++v;
}

glm::mat4 GetModel(const glm::mat4& m, const glm::vec3& pos, float rot, const glm::vec3& pivot, float scale = 1.f);
void AABBToBBoxEdgesCCW(const AABB& aabb, const glm::mat4& m, std::vector<glm::vec3>& res);
void AABBToBBoxEdgesCCW(const AABB& aabb, std::vector<glm::vec3>& res);
AABB TransformAABB(const AABB& aabb, const glm::mat4& m);
using BBox = std::array<glm::vec3, 4>;
BBox TransformBBox(const AABB& aabb, const glm::mat4& m);

template<typename T>
class Val {
	T val;
public:
	const T prev;
	Val(T& val) : val(val), prev(val) {}
	Val& operator=(const T& val) { const_cast<T&>(prev) = this->val; this->val = val; return *this; }
	operator const T&() { return val; }
};
//template<typename T>
//void EraseAll(const std::vector<T>& v, ) {
//	std::remove_if()
//}
