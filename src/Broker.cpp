// BrokerImpl.cpp

// Brokerens CreateTable funktion kan kun t�le en tr�d ad gangen. Skal enforces (mhb p� services)

#include <list>
#include <cstring>
#include <algorithm>

//#pragma comment(lib,"sqlite3.lib")

#define _BROKERIMP

#include "Broker.h"
#include "TextStream.h"

#include "sqlite/Source/sqlite3.h"


using namespace std;


_BROKERIMP const char* SQLType(const int&)           { return "integer"; }
_BROKERIMP const char* SQLType(const unsigned int&)  { return "integer"; }
_BROKERIMP const char* SQLType(const long&)          { return "integer"; }
_BROKERIMP const char* SQLType(const unsigned long&) { return "integer"; }
_BROKERIMP const char* SQLType(const double&)        { return "real"; }
_BROKERIMP const char* SQLType(const char*)          { return "text"; }
//_BROKERIMP const char* SQLType(const int64_t&)       { return "integer"; }


static sqlite3* db = 0;

_BROKERIMP bool OpenDataBase(const std::string& path) {
	return sqlite3_open(path.c_str(),&db) == SQLITE_OK;
}

_BROKERIMP void CloseDataBase() {
	sqlite3_close(db); // SQLITE_OK
	db = 0;
}

static bool transaction = false;

_BROKERIMP bool BeginTransaction() {
	char *zErrMsg = 0;
  if(sqlite3_exec(db, "begin EXCLUSIVE transaction anomyn", 0, 0, &zErrMsg) !=SQLITE_OK ){
    sqlite3_free(zErrMsg);
		return false;
  }
	return transaction = true;
}

_BROKERIMP bool Commit() {
	char *zErrMsg = 0;
  if(sqlite3_exec(db, "commit transaction noname", 0, 0, &zErrMsg) != SQLITE_OK ){
    sqlite3_free(zErrMsg);
		transaction = false;
		return false;
  }
	transaction = false;
	return true;
}

_BROKERIMP void RollBack() {
	char *zErrMsg = 0;
  if(sqlite3_exec(db, "rollback transaction noname", 0, 0, &zErrMsg) != SQLITE_OK ){
    sqlite3_free(zErrMsg);
  }
	transaction = false;
}

_BROKERIMP bool Transaction() {
	return transaction;
}

// ---- Buffer ----

Buffer::Buffer(const char* column_name, int is_key) :
	usage(is_key), column(0) {
	if (column_name) {
    column = new char[strlen(column_name)+1];
    strcpy(column, column_name);
	}
}

Buffer::~Buffer() {
	if (column) delete [] column;
}

ostream& Buffer::CreateStatement(ostream& SQL) const {
	SQL << column << " " << SQLType();
	if (is_key())
		SQL << " primary key";
	else if (IsNumeric())
		SQL << " default 0";
	return SQL;
}

// ---- IBroker -----

IBroker::~IBroker() {}


// ---- Cursor ----

class SQLiteStatement : public Cursor {
public:
	SQLiteStatement(sqlite3_stmt* pStmt) : pStmt(pStmt) {}
private:
	bool BindBuffer(int pos, int value)              { return sqlite3_bind_int(pStmt, pos, value)    == SQLITE_OK; }
	bool BindBuffer(int pos, unsigned int value)     { return sqlite3_bind_int(pStmt, pos, value)    == SQLITE_OK; }
	bool BindBuffer(int pos, long value)             { return sqlite3_bind_int(pStmt, pos, value)    == SQLITE_OK; }
	bool BindBuffer(int pos, unsigned long value)    { return sqlite3_bind_int(pStmt, pos, value)    == SQLITE_OK; }
	bool BindBuffer(int pos, double value)           { return sqlite3_bind_double(pStmt, pos, value) == SQLITE_OK; }
	bool BindBuffer(int pos, const char* value)      { return sqlite3_bind_text(pStmt, pos, value, -1, SQLITE_TRANSIENT) == SQLITE_OK; }
//	bool BindBuffer(int pos, int64_t value)          { return sqlite3_bind_int64(pStmt, pos, value)  == SQLITE_OK; }
	void Fetch(int pos, int& var)                    { var = sqlite3_column_int(pStmt, pos); }
	void Fetch(int pos, unsigned int& var)           { var = sqlite3_column_int(pStmt, pos); }
	void Fetch(int pos, long& var)                   { var = sqlite3_column_int(pStmt, pos); }
	void Fetch(int pos, unsigned long& var)          { var = sqlite3_column_int(pStmt, pos); }
	void Fetch(int pos, double& var)                 { var = sqlite3_column_double(pStmt, pos); }
	void Fetch(int pos, const char*& var)            { var = reinterpret_cast<const char*>(sqlite3_column_text(pStmt, pos)); }
//	void Fetch(int pos, int64_t& var)                { var = sqlite3_column_int64(pStmt, pos); }
	sqlite3_stmt* pStmt;
};

// ---- BrokerImpl ----

class BrokerImpl : public IBroker {
public:
	BrokerImpl(const char* table, Broker& broker);
	~BrokerImpl();
	bool  CreateTable(const char*& errmsg);
	bool  Execute(const char* SQL,const char*& errmsg);
	bool  Select(const char* SQL,const char*& errmsg);
	bool  AutoSelect(const char*& errmsg, const char* where_clause = 0, const char* order_by = 0);
	bool  InsertThing(const void* thing,const char*& errmsg);
	bool  UpdateThing(const void* thing,const char*& errmsg);
	bool  DeleteThing(const void* thing,const char*& errmsg);
	void  CreateIt()               { broker.CreateIt(); }
	void* CreateThing()            { return broker.CreateThing(); }
	void  HandleThing(void* thing) { broker.HandleThing(thing); }
	void  Append(Buffer* buffer);
private:
	Buffer* LookupBuffer(const char* column);
	bool    AlterTable(const char*& errmsg);
	bool    BindBuffers(const void* thing,Cursor* cursor, bool keys_only) const;
	void    ConstructCreateTable(ostream& SQL);
	void    ConstructAlterTable(ostream& SQL, const Buffer& buffer);
	void    ConstructInsert(ostream& SQL, const void* thing);
	void    ConstructUpdate(ostream& SQL, const void* thing);
	void    ConstructDelete(ostream& SQL, const void* thing);
	// State
	std::list<Buffer*> buffers;
	std::string        table;
	Broker&            broker;
	std::list<Buffer*> table_buffers;
	BrokerImpl* previous_broker;
friend
	int callback(void*, int, char**, char**);
};

static BrokerImpl* current_broker = 0;  // No reentrence. Men det m� den ikke for sqlite aligevel. Skal vi have sikret

BrokerImpl::BrokerImpl(const char* _table, Broker& broker) : broker(broker), previous_broker(0) {
	if (_table) table = _table;
	previous_broker = current_broker;
	current_broker = this;
}

BrokerImpl::~BrokerImpl() {
	// delete all buffers
	for (std::list<Buffer*>::const_iterator i=buffers.begin(); i!=buffers.end(); ++i)
		delete *i;
	current_broker = previous_broker;
}

Buffer* BrokerImpl::LookupBuffer(const char* column) {
	for (std::list<Buffer*>::const_iterator i=buffers.begin(); i!=buffers.end(); ++i) {
    Buffer& buffer = **i;
		if (strcmp(buffer.column,column) == 0) return &buffer;
	}
	return 0;
}

int callback(void* NotUsed, int argc, char** argv, char** azColName){
	// Match table.columns mod broker.buffers
	Buffer* buffer = 0;
  for(int i=0; i<argc; i++) {
		if (!azColName[i]) continue;
		if (strcmp(azColName[i],"name")!=0) continue;
		buffer = current_broker->LookupBuffer(argv[i]);
		if (buffer) break;
  }
	if (buffer) current_broker->table_buffers.push_back(buffer);
  return 0;
}

bool BrokerImpl::CreateTable(const char*& errmsg) {
	textstream SQL;
	ConstructCreateTable(SQL);
	if (!Execute(SQL,errmsg)) return false;
  // Fetch table column info.
	static char msg[128];
  char *zErrMsg = 0;
	textstream pragma;
	pragma << "PRAGMA table_info("<<table<<")" << ends;
  if(sqlite3_exec(db, pragma, callback, 0, &zErrMsg) !=SQLITE_OK ){
		strcpy(msg,zErrMsg);
		errmsg = msg;
    sqlite3_free(zErrMsg);
		return false;
  }
	return AlterTable(errmsg);
}

bool BrokerImpl::AlterTable(const char*& errmsg) {
  list<Buffer*> to_append;
	for (list<Buffer*>::iterator i=buffers.begin(); i!=buffers.end(); ++i) {
		if (find(table_buffers.begin(),table_buffers.end(),*i) == table_buffers.end()) to_append.push_back(*i);
	}
	if (to_append.size() == 0) return true; // Nothing to do
	for (list<Buffer*>::iterator i=to_append.begin(); i!=to_append.end(); ++i) {
		textstream SQL;
		ConstructAlterTable(SQL,**i);
		if (!Execute(SQL,errmsg)) return false;
	}
	to_append.clear();
	return true;
}

void BrokerImpl::Append(Buffer* buffer) {
	buffers.push_back(buffer);
}

bool BrokerImpl::BindBuffers(const void* thing,Cursor* cursor, bool keys_only) const {
  for (std::list<Buffer*>::const_iterator i=buffers.begin(); i!=buffers.end(); ++i) {
		Buffer& buffer = **i;
		// If keys only then skip buffers that are not keys
		if (keys_only && !buffer.is_key()) continue;
		// Bind by position
		if (!buffer.BindBuffer(thing,*cursor)) return false;
  }
  return true;
}

bool BrokerImpl::InsertThing(const void* thing,const char*& errmsg) {
	textstream SQL;
	ConstructInsert(SQL, thing);

	const char* szTail=0;
	sqlite3_stmt* pStmt;
	if(sqlite3_prepare_v2(db,SQL,-1,&pStmt,&szTail) != SQLITE_OK ){
		errmsg = sqlite3_errmsg(db);
		return false;
  }
	SQLiteStatement statement(pStmt);
	if (!BindBuffers(thing,&statement,false)) {
		errmsg = sqlite3_errmsg(db);
		return false;
	}
	int result = sqlite3_step(pStmt);
	if (!(result == SQLITE_OK || result == SQLITE_DONE)) {
		errmsg = sqlite3_errmsg(db);
		return false;
  }
	sqlite3_finalize(pStmt);

	return true;
}

bool BrokerImpl::UpdateThing(const void* thing,const char*& errmsg) {
	textstream SQL;
	ConstructUpdate(SQL, thing);

	const char* szTail=0;
	sqlite3_stmt* pStmt;
	if(sqlite3_prepare_v2(db,SQL,-1,&pStmt,&szTail) != SQLITE_OK ){
    errmsg = sqlite3_errmsg(db);
		return false;
  }
	SQLiteStatement statement(pStmt);
	if (!BindBuffers(thing,&statement,false)) {
		errmsg = sqlite3_errmsg(db);
		return false;
	}
	int result = sqlite3_step(pStmt);
	if (!(result == SQLITE_OK || result == SQLITE_DONE)) {
		errmsg = sqlite3_errmsg(db);
		return false;
	}
	sqlite3_finalize(pStmt);

	return true;
}

bool BrokerImpl::DeleteThing(const void* thing,const char*& errmsg) {
	textstream SQL;
	ConstructDelete(SQL, thing);

	const char* szTail=0;
	sqlite3_stmt* pStmt;
	if(sqlite3_prepare_v2(db,SQL,-1,&pStmt,&szTail) != SQLITE_OK ){
    errmsg = sqlite3_errmsg(db);
		return false;
  }
	SQLiteStatement statement(pStmt);
	if (!BindBuffers(thing,&statement,true)) {
		errmsg = sqlite3_errmsg(db);
		return false;
	}
	int result = sqlite3_step(pStmt);
	if (!(result == SQLITE_OK || result == SQLITE_DONE)) {
		errmsg = sqlite3_errmsg(db);
		return false;
	}
	sqlite3_finalize(pStmt);
	return true;
}

bool BrokerImpl::Execute(const char* SQL, const char*& errmsg) {
	const char* szTail=0;
	sqlite3_stmt* pStmt;
	if(sqlite3_prepare_v2(db,SQL,-1,&pStmt,&szTail) != SQLITE_OK ) {
		errmsg = sqlite3_errmsg(db);
		return false;
  }
	int result = sqlite3_step(pStmt);
	if (!(result == SQLITE_OK || result == SQLITE_DONE)) {
		errmsg = sqlite3_errmsg(db);
		return false;
	}
	sqlite3_finalize(pStmt);

	return true;
}

bool BrokerImpl::Select(const char* SQL,const char*& errmsg) {
	// Buffer position used in fetching the col in the resultset
	int pos = 0;
  for (std::list<Buffer*>::const_iterator i=buffers.begin(); i!=buffers.end(); ++i) {
		Buffer& buffer = **i;
    // Assign position to buffer
    buffer.pos = pos++;
  }
  // Prepare the statement
	const char* szTail=0;
	sqlite3_stmt* pStmt;
	if(sqlite3_prepare_v2(db,SQL,-1,&pStmt,&szTail) != SQLITE_OK ){
    errmsg = sqlite3_errmsg(db);
		return false;
  }
	// Fetch
	SQLiteStatement cursor(pStmt);
	int result;
	while (result = sqlite3_step(pStmt) == SQLITE_ROW) {
		// Create new object
		void* thing = CreateThing();
		// Transfer data from buffers to thing / variables
		for (std::list<Buffer*>::const_iterator i=buffers.begin(); i!=buffers.end(); ++i) {
			Buffer& buffer = **i;
			buffer.Fetch(thing,cursor);
		}
		thing ? HandleThing(thing) : CreateIt();
	}
	if (result != SQLITE_OK) {
		errmsg = sqlite3_errmsg(db);
		return false;
	}
	sqlite3_finalize(pStmt);

	return true;
}


bool BrokerImpl::AutoSelect(const char*& errmsg, const char* where_clause, const char* order_by) {
	ostringstream SQL;
	// Build SELECT list
  SQL << "SELECT ";
  for (std::list<Buffer*>::const_iterator i=buffers.begin(); i!=buffers.end(); ++i) {
		Buffer& buffer = **i;
		// Add column name to select list
		SQL << buffer.column << ", ";
	}
	// Erase the last comma
	SQL.seekp(-2, ios::cur);
	// Build FROM list
  SQL << " FROM " << table;
  if (where_clause)
    SQL << " WHERE " << where_clause;
	if (order_by)
    SQL << " ORDER BY " << order_by;
  SQL << ends;
  // K�r svinet
  return Select(SQL.str().c_str(),errmsg);
}

void BrokerImpl::ConstructCreateTable(ostream& SQL) {
	// Build create table statementt
  SQL << "create table if not exists " << table << " (";
  for (std::list<Buffer*>::const_iterator i=buffers.begin(); i!=buffers.end(); ++i) {
		Buffer& buffer = **i;
		// Add column name to select list
		buffer.CreateStatement(SQL) << ", ";
	}
	// Erase the last comma
	SQL.seekp(-2, ios::cur);
  SQL << ")";
  SQL << ends;
}

void BrokerImpl::ConstructAlterTable(ostream& SQL, const Buffer& buffer) {
	// Build create table statementt
  SQL << "alter table " << table << " add column ";
	buffer.CreateStatement(SQL);
  SQL << ends;
}

void BrokerImpl::ConstructInsert(ostream& SQL, const void* thing) {
	// All columns must have names!
	SQL << "INSERT INTO " << table << " (";

	for (std::list<Buffer*>::iterator i=buffers.begin(); i!=buffers.end(); ++i) {
		Buffer& buffer = **i;
		// Add column name to list
		if (i!=buffers.begin()) SQL << ", ";
		SQL << buffer.column;
  }
	int pos = 1;
	SQL << ") VALUES (";
	for (std::list<Buffer*>::iterator i=buffers.begin(); i!=buffers.end(); ++i) {
		Buffer& buffer = **i;
		buffer.pos = pos++;
		if (i!=buffers.begin()) SQL << ", ";
    SQL << "?";
  }
	SQL << ")" << std::ends;
}

void BrokerImpl::ConstructUpdate(ostream& SQL, const void* thing) {
	SQL << "UPDATE " << table << " SET ";
  int pos = 1;
  // Construct 'SET' part
	for (std::list<Buffer*>::iterator i=buffers.begin(); i!=buffers.end(); ++i) {
		Buffer& buffer = **i;
    // Skip key buffers (they go into the WHERE clause)
    if (buffer.is_key()) continue;
    // Bind by number
    buffer.pos = pos++;
    SQL << buffer.column << " = ?, ";
  }
	// Delete the last comma
  SQL.seekp(-2, ios::cur);
  // Construct the WHERE clause
  SQL << " WHERE ";
	for (std::list<Buffer*>::iterator i=buffers.begin(); i!=buffers.end(); ++i) {
		Buffer& buffer = **i;
		if (!buffer.is_key()) continue;
    // Bind by number
    buffer.pos = pos++;
    SQL << buffer.column << " = ? AND ";
  }
  // Delete last 'AND'
  SQL.seekp(-5, ios::cur).put('\0');
	// OK, done!
}

void BrokerImpl::ConstructDelete(ostream& SQL, const void* thing) {
	SQL << "DELETE FROM " << table << " WHERE ";
  int pos = 1;
	for (std::list<Buffer*>::iterator i=buffers.begin(); i!=buffers.end(); ++i) {
		Buffer& buffer = **i;
		// Build where clause from keys
		if (buffer.is_key()) {
      // Bind by number
      buffer.pos = pos++;
      SQL << buffer.column << " = ? AND ";
    }
  }
  // Delete last 'AND'
  SQL.seekp(-5, ios::cur).put('\0');
	// OK, done!
}

//---- Broker ----

Broker::Broker(const char* table) {
	impl = new BrokerImpl(table,*this);
}

Broker::~Broker() {
	delete impl;
}

bool Broker::DoCreateTable(const char*& errmsg) {
	return impl->CreateTable(errmsg);
}

bool Broker::DoExecute(const char* SQL, const char*& errmsg) {
	return impl->Execute(SQL,errmsg);
}

bool Broker::DoSelect(const char* SQL,const char*& errmsg) {
	return impl->Select(SQL,errmsg);
}

bool Broker::DoAutoSelect(const char*& errmsg, const char* where_clause, const char* order_by) {
	return impl->AutoSelect(errmsg,where_clause,order_by);
}

bool Broker::InsertThing(const void* thing,const char*& errmsg) {
	return impl->InsertThing(thing,errmsg);
}

bool Broker::UpdateThing(const void* thing,const char*& errmsg) {
	return impl->UpdateThing(thing,errmsg);
}

bool Broker::DeleteThing(const void* thing,const char*& errmsg) {
	return impl->DeleteThing(thing,errmsg);
}

void Broker::Append(Buffer* buffer) {
	impl->Append(buffer);
}


// BrokerImpl.cpp
