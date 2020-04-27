// SQLBroker.h

#pragma once

#include "Broker.h"
#include "SimpleBuffer.h"
#include "BrokerException.h"


class SQLBroker : public Broker { //BrokerExecuteImpl<SQLBroker> {
public:
  SQLBroker(const wchar_t* table = 0) : Broker(CW2A(table,CP_UTF8)) {} 
	template <typename T>
  void MapBuffer(T& var) { 
    Append(new SimpleBuffer<T,DefaultConverter<T>>(var, 0)); 
  }
	template <typename T,typename C>
  void MapBuffer(T& var,C converter) {
		Append(new SimpleBuffer<T,C>(var, 0)); 
	}
	void MapBuffer(std::wstring& var) {
		Append(new SimpleStringBuffer(var));    
  }
	void Execute(const wchar_t* SQL) { 
		const char* errmsg;
		if (!DoExecute(CW2A(SQL,CP_UTF8),errmsg)) throw BrokerException(L"SQLBroker",SQL,errmsg);  
	}
	void Select(const wchar_t* SQL) { 
		const char* errmsg;
		if (!DoSelect(CW2A(SQL,CP_UTF8),errmsg)) throw BrokerException(L"SQLBroker",SQL,errmsg);  
	}
protected:
	virtual void OnCreate() {}
private:
	void  CreateIt()         { OnCreate(); } // noop 
  void* CreateThing()      { return 0; }
  void  HandleThing(void*) {}
};

// SQLBroker.h