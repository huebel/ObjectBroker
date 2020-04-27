// ObjectBroker.h

#pragma once

#include "Broker.h"
#include "MemberBuffer.h"
#include "BrokerException.h"
#include <assert.h>


template <typename T>
class ObjectBroker : public Broker { //Impl<ObjectBroker<T>> {
public:
	ObjectBroker(const char* table)
	:	Broker(table)
	,	table_name(table)
	{ assert(table); }
	template <typename M>
	void MapKeyMember(M T::*member, const char* column_name) {
		Append(new MemberBuffer<T,M,DefaultConverter<M>>(column_name, member, 1)); 
	}
	template <typename M>
	void MapMember(M T::*member, const char* column_name) {
		Append(new MemberBuffer<T,M,DefaultConverter<M>>(column_name, member, 0));
	}
	template <typename M,typename C>
	void MapMember(M T::*member, const char* column_name,C converter) {
		Append(new MemberBuffer<T,M,C>(column_name, member, 0)); 
	}
	void MapMember(std::wstring T::*member, const char* column_name) {
		Append(new StringMemberBuffer<T>(column_name, member));  
	}
	void CreateTable() {
		const char* errmsg;
		if (!DoCreateTable(errmsg)) throw BrokerException(table_name.c_str(),"CreateTable",errmsg);
	}
	void Execute(const char* SQL) {
		const char* errmsg;
		if (!DoExecute(SQL, errmsg)) throw BrokerException(table_name.c_str(),"Execute",errmsg);
	}
	void Select(const char* where_clause = 0, const char* order_by = 0) {
		const char* errmsg;
		if (!DoAutoSelect(errmsg, where_clause, order_by)) throw BrokerException(table_name.c_str(),"select",errmsg);
	}
	void Insert(const T & object) {
		const char* errmsg;
		if (!InsertThing(&object,errmsg)) throw BrokerException(table_name.c_str(),"Insert",errmsg);
	}
	void Update(const T & object) {
		const char* errmsg;
		if (!UpdateThing(&object,errmsg)) throw BrokerException(table_name.c_str(),"Update",errmsg);
	}
	void Delete(const T & object) {
		const char* errmsg;
		if (!DeleteThing(&object,errmsg))  throw BrokerException(table_name.c_str(),"Delete",errmsg);
	}
protected:
	virtual T*   CreateObject()   = 0;
	virtual void HandleObject(T&) = 0;
	std::string  table_name;
private:
	void  CreateIt()               {} // noop 
	void* CreateThing()            { return CreateObject(); }
	void  HandleThing(void* thing) { HandleObject(*(T*)thing); }
};

// ObjectBroker.h
