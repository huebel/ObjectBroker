// BrokerException.h

#pragma once

#include <sstream>

class BrokerException {
public:
	BrokerException(const char* table, const char* operation, const char* errmsg) {
		std::string err(errmsg);
		std::ostringstream os;
		os << table << "\n" << operation << "\n" << err.c_str() << std::ends;
		msg = os.str();
	}
	operator const char* () const { return msg.c_str(); }
private:
	std::string msg;
};

// BrokerException.h
