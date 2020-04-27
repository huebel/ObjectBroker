// DefaultConverter.h

#pragma once

template <typename T>
class DefaultConverter {
public:
	const T& ToDB(const T& source) const   { return source; }
	const T& FromDB(const T& source) const { return source; }
	const T& Type() const                  { return buffer; }
	T buffer;
};

class StringConverter {
public:
	const char*    ToDB(const std::string& source) { tmp = source; return tmp.c_str(); }
	const char*    FromDB(const char* source)       { tmp = source ? source : ""; return tmp.c_str(); }
	const char*    Type() const                     { return buffer; }
	const char* buffer;
	std::string tmp;
};

// DefaultConverter.h
