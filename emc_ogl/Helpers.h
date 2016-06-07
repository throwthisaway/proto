#pragma once
inline auto RoundToPowerOf2(unsigned int v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return ++v;
}

//template<typename T>
//void EraseAll(const std::vector<T>& v, ) {
//	std::remove_if()
//}
