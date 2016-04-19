#pragma once
#include <string>
#define EXCEPTIONS
#ifdef EXCEPTIONS
class custom_exception : public std::exception {
	const char* str;
	const std::string message;
public:
	explicit custom_exception(const char* str) : str(str) {}
	explicit custom_exception(const std::string& message) : str(nullptr), message(message) {}
	const char * what() const _NOEXCEPT override { return str ? str : message.c_str(); }
};
void ThrowIf(bool exp, const char* msg);
#endif
