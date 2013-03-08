#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cassert>
#include <cstdlib>
#include <cwchar>
#include <cerrno>

#ifndef _UNICODE_CONV_H
#define _UNICODE_CONV_H

namespace UniConv{
// Dummy overload
std::wstring get_wstring(const std::wstring & s);

// Real worker
std::wstring get_wstring(const std::string & s);

// Dummy
std::string get_string(const std::string & s);

// Real worker
std::string get_string(const std::wstring & s);

} // UniConv
#endif // _UNICODE_CONV_H