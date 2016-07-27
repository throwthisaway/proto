#pragma once
#include <vector>
#include <glm/glm.hpp>

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

struct AABB;
void AABBToBBoxEdgesCCW(const AABB& aabb, const glm::mat4& m, std::vector<glm::vec3>& res);
//template<typename T>
//void EraseAll(const std::vector<T>& v, ) {
//	std::remove_if()
//}
