// Broker.h

#pragma once

#include <string>
#include <cstdint>

/* Define _BROKERIMP */

#ifndef _BROKERIMP
#if defined(__WINDOWS__)
#define _BROKERIMP __declspec(dllimport)
#else
#define _BROKERIMP
#endif
#endif 

extern _BROKERIMP bool OpenDataBase(const std::string& path);
extern _BROKERIMP void CloseDataBase();
extern _BROKERIMP bool BeginTransaction();
extern _BROKERIMP bool Commit();
extern _BROKERIMP void RollBack();
extern _BROKERIMP bool Transaction();

// Gem i en anden header
extern _BROKERIMP const char* SQLType(const int&);
extern _BROKERIMP const char* SQLType(const unsigned int&);
extern _BROKERIMP const char* SQLType(const long&);
extern _BROKERIMP const char* SQLType(const unsigned long&);
extern _BROKERIMP const char* SQLType(const double&);
extern _BROKERIMP const char* SQLType(const char*);
//extern _BROKERIMP const char* SQLType(const int64_t&);

inline bool IsNumeric(const int&)           { return true; }
inline bool IsNumeric(const unsigned int&)  { return true; }
inline bool IsNumeric(const long&)          { return true; }
inline bool IsNumeric(const unsigned long&) { return true; }
inline bool IsNumeric(const double&)        { return true; } 
inline bool IsNumeric(const char*)          { return false; }
//inline bool IsNumeric(const int64_t&)       { return true; }


class Cursor {
public:
	virtual ~Cursor() {  }
	virtual bool BindBuffer(int pos, int value)              = 0;
	virtual bool BindBuffer(int pos, unsigned int value)     = 0;
	virtual bool BindBuffer(int pos, long value)             = 0;
	virtual bool BindBuffer(int pos, unsigned long value)    = 0;
	virtual bool BindBuffer(int pos, double value)           = 0;
	virtual bool BindBuffer(int pos, const char* value)      = 0;
//	virtual bool BindBuffer(int pos, int64_t value)          = 0;
	virtual void Fetch(int pos, int& var)                    = 0;
	virtual void Fetch(int pos, unsigned int& var)           = 0;
	virtual void Fetch(int pos, long& var)                   = 0;
	virtual void Fetch(int pos, unsigned long& var)          = 0;
	virtual void Fetch(int pos, double& var)                 = 0;
	virtual void Fetch(int pos, const char*& var)            = 0;
//	virtual void Fetch(int pos, int64_t& var)                = 0;
	template<class Enum,class=typename std::enable_if< std::is_enum<Enum>::value>::type>
	void Fetch(int pos, Enum& var)
	{
		static_assert(sizeof(Enum)==sizeof(int));
		Fetch(pos, reinterpret_cast<int&>(var));
	}
};


class _BROKERIMP Buffer {
public:
	enum use_buffer { use_select = 0x0000, use_key = 0x0001 };
	Buffer(const char* column_name, int is_key);
	virtual ~Buffer();
	bool         is_select() const { return usage == use_select; }
	bool         is_key() const    { return usage & use_key; }
	virtual bool BindBuffer(const void* thing,Cursor& cursor) = 0; 
	virtual void Fetch(void* thing, Cursor& cursor)           = 0; 
	virtual const char* SQLType() const                       = 0;
	virtual bool IsNumeric() const                            = 0;
	std::ostream& CreateStatement(std::ostream& SQL) const;
	// State
	char*     column;
	const int usage;
	int       pos;
};


class IBroker { // Kan den ikke komme ned i cpp filen
public:	
	virtual ~IBroker() = 0;
	virtual bool CreateTable(const char*& errmsg)                   = 0;
	virtual bool Execute(const char* SQL,const char*& errmsg)       = 0;
	virtual bool Select(const char* SQL,const char*& errmsg)        = 0;
	virtual bool AutoSelect(const char*& errmsg, const char* where_clause, const char* order_by) = 0; 
	virtual bool InsertThing(const void* thing,const char*& errmsg) = 0;
	virtual bool UpdateThing(const void* thing,const char*& errmsg) = 0;
	virtual bool DeleteThing(const void* thing,const char*& errmsg) = 0;
	virtual void Append(Buffer* buffer)                             = 0;
};


class _BROKERIMP Broker {
public:	
	~Broker();
	virtual void  CreateIt()               = 0;
	virtual void* CreateThing()            = 0;
	virtual void  HandleThing(void* thing) = 0;
protected:
	Broker(const char* table);
	void Append(Buffer* buffer);
	bool DoExecute(const char* SQL,const char*& errmsg); 
	bool DoCreateTable(const char*& errmsg);
	bool DoSelect(const char* SQL,const char*& errmsg);
	bool DoAutoSelect(const char*& errmsg,const char* where_clause, const char* order_by); 	
	bool InsertThing(const void* thing,const char*& errmsg);
	bool UpdateThing(const void* thing,const char*& errmsg);
	bool DeleteThing(const void* thing,const char*& errmsg);
	// State
	IBroker* impl;
};

// Broker.h
