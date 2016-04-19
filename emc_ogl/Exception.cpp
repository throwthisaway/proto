#include "Exception.h"
void ThrowIf(bool exp, const char* msg) {
	if (exp) throw custom_exception(msg);
}