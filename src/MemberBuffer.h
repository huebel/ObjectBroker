// MemberBuffer.h

#pragma once

#include <string>
#include <functional>
#include "Broker.h"
#include "DefaultConverter.h"


template <typename T, typename M, typename C> // Object type, member type
class MemberBuffer : public Buffer, private C {
	using C::buffer;
public:
	MemberBuffer(const char* column_name, M T::*member, int buffer_usage)
	: Buffer(column_name, buffer_usage)
	, ptm(member)
	{}
private:
	bool BindBuffer(const void* thing,Cursor& cursor) {
		T* object = (T*)thing;
		return cursor.BindBuffer(pos, C::ToDB(object->*ptm));
	}
	void Fetch(void* thing,Cursor& cursor) {
		cursor.Fetch(pos,C::buffer);
		T* object = (T*)thing;
		object->*ptm = C::FromDB(C::buffer);
	}
	const char* SQLType() const   { return ::SQLType(C::Type()); }
	bool        IsNumeric() const { return ::IsNumeric(C::Type()); }
  // State
  M T::*ptm;
};


template <typename T>
class StringMemberBuffer : public MemberBuffer<T,std::string,StringConverter> {
public:
	StringMemberBuffer(const char* column_name, std::string T::*member)
	: MemberBuffer<T,std::string,StringConverter>(column_name, member, 0)
	{}
};

// MemberBuffer.h
