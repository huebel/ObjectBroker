// TextStream.h

#pragma once

#include <sstream>

//using namespace std; 

class textstream : public std::ostream {
public:
	textstream() : std::ostream(&_buf) {}
	// Get the string (ensures zero termination and resets string)
  operator const char* () { put('\0'); seekp(0); return _buf.pbase(); }
	// Return the length of the string
	size_t length() { return _buf.str().size(); }
private:
	class textstreambuf : public std::stringbuf {
	public:
		char* pbase() { return std::stringbuf::pbase(); }
	} _buf;
};

// TextStream.h