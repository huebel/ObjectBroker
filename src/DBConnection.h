// DataBaseConnection.h

#pragma once

#include <Broker.h>

inline bool DBConnect(const wchar_t* path) {
	return OpenDataBase(std::string(CW2A(path,CP_UTF8)));
}

inline void DBClose() {
	CloseDataBase();
}

// DataBaseConnection.h