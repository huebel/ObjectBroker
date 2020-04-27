// SimpleBuffer.h

#pragma once

#include "Broker.h"
#include <string>
#include <DefaultConverter.h>


template <typename T, typename C=DefaultConverter<T>> 
class SimpleBuffer : public Buffer, private C {
public:
	SimpleBuffer(T& var, int buffer_usage) : Buffer("", buffer_usage), var(var) {}
private:
	bool BindBuffer(const void*,Cursor& cursor) {
		return cursor.BindBuffer(pos, ToDB(var)); 
	}
	void Fetch(void*,Cursor& cursor) {
    cursor.Fetch(pos,buffer);
		var = FromDB(buffer);
	}
	const char* SQLType() const   { return ::SQLType(Type()); }
	bool        IsNumeric() const { return ::IsNumeric(Type()); }
  // State
  T& var;
};


class SimpleStringBuffer : public SimpleBuffer<std::wstring,StringConverter> {
public:
	SimpleStringBuffer(std::wstring& var) : SimpleBuffer(var,0) {}
};

// SimpleBuffer.h