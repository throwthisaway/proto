#pragma once
#include "AABB.h"
#include <vector>
bool HitTest(const std::vector<glm::vec3>&, const std::vector<glm::vec3>&);
bool HitTest(const AABB&, const std::vector<glm::vec3>&);
void TestHitTest();
