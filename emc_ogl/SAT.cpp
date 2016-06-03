#include "SAT.h"
#include <algorithm>
namespace {
	inline auto NormalCW(const glm::vec3& vec) {
		return glm::vec3(vec.y, -vec.x, 0.f);
	}
	std::pair<float, float> ProjectMinMax(const glm::vec3& n, const std::vector<glm::vec3>& vertices) {
		std::pair<float, float> res;
		for (size_t i = 0; i < vertices.size(); ++i) {
			const float pvn = glm::dot(n, vertices[i]);
			if (i == 0) { res.first = res.second = pvn; continue; }
			res.first = std::min(res.first, pvn); res.second = std::max(res.second, pvn);
		}
		return res;
	}
	float Check(const std::vector<glm::vec3>& src, const std::vector<glm::vec3>& dst) {
		for (size_t s = 0; s < src.size() - 1; s+=2) {
			const auto n = glm::normalize(NormalCW(src[s+1] - src[s]));
			const auto resSrc = ProjectMinMax(n, src), resDst = ProjectMinMax(n, dst);
			const auto d = (std::max(resSrc.second, resDst.second) - std::min(resSrc.first, resDst.first))
				- (resSrc.second - resSrc.first + resDst.second - resDst.first);
			if (d > 0.f)
				return d;
		}
		return 0.f;
	}
}
bool HitTest(const std::vector<glm::vec3>& v1, const std::vector<glm::vec3>& v2) {
	if (Check(v2, v1)>0.f) return false;
	return Check(v1, v2)<=0.f;
}
bool HitTest(const AABB& bounds, const std::vector<glm::vec3>& v) {
	return HitTest({ { bounds.l, bounds.t, 0.f },{ bounds.r, bounds.t, 0.f },
	{ bounds.r, bounds.t, 0.f },{ bounds.r, bounds.b, 0.f },
	{ bounds.r, bounds.b, 0.f },{ bounds.l, bounds.b, 0.f },
	{ bounds.l, bounds.b, 0.f },{ bounds.l, bounds.t, 0.f } }, v);
}

void TestHitTest() {
	bool res;
	res = HitTest({ 0., 1., 1., 0. }, { { 0., 0., 0. },{ .5, .5, .0 } }); // simple containment
	assert(res);
	res = HitTest({ 0., 1., 1., 0. }, { { .5, -.5, 0. },{ 1.5, .5, .0 } }); // hit bottom-right
	assert(res);
	res = HitTest({ 0., 1., 1., 0. }, { { .5, -.5, 0. },{ 1.7, .3, .0 } }); // miss bottom-right
	assert(!res);
}