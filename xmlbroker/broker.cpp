// Projekt.......: BROKER
// Navn..........: broker.cpp
// Ansvarlig.....: TN/PH

// $Header:$

#include <windows.h>

#include <exception>

#include "database.h"
#include "textstream.h"
// #include "mboxstream.h"

#include <tools/trace.h>

//#pragma warning(disable:4996)

#ifdef _DEBUG
	//#define TRACE_ERROR(msg) do { mboxstream("Broker Error") << msg << show; } while (0)
	#define TRACE_ERROR(msg) do { TRACE("Broker Error: %s\n", msg); } while (0)
#else
	#define TRACE_ERROR(msg)
#endif


//void inline DisplaySQL(const char* SQL)
//{
//  // FIXME: report errors how?
//  mboxstream("Broker SQL") << SQL << show;
//}

int inline user_skip() { return GetAsyncKeyState(VK_CONTROL) < 0; }

#include "broker_impl.h"

#include "broker.h"

/* Cursor handler */

int show_sql = 0;
int show_bufsizes = 0;
int paste_error = 1;

using namespace std;

template <class T>
class ScopeTempValue {
  T& variable;
  T original_value;
public:
  ScopeTempValue(T& var, T temporary_value) : variable(var), original_value(var) { variable = temporary_value; }
  ~ScopeTempValue() { variable = original_value; }
};

// Built in types

/* Data types */

#define IDST_ORACLE_ERROR "Database error"

// Buffer class

void* BufferBase::operator new(size_t sz)
{
	return ::operator new(sz);
}

void BufferBase::operator delete(void* addr)
{
	::operator delete(addr);
}

BufferBase::BufferBase(Broker& broker, const char* column_name, size_t size, int type_code, int usage_type)
: broker(broker)
, sz(size)
, next(0)
, type(type_code)
, usage(usage_type)
, pos(0)
{
  // Copy column name
  if (column_name) {
    column = new char[strlen(column_name)+1];
    strcpy(column, column_name);
  }
  else {
    column = 0;
  }
  // Append 'this' to the list in Broker
  if (!broker.first) {
    broker.first = broker.last = this;
  }
  else {
    broker.last->next = this;
    broker.last = this;
  }
  // Create buffers
  size_t bufsiz = size * broker.buffer_length;
  size_t nulsiz = sizeof(DWORD) * broker.buffer_length;
  data = new char[bufsiz+nulsiz];
  if (data) {
    memset(data, 0, bufsiz+nulsiz);
    status = (data+bufsiz);
  }
  else {
    status = 0;
  }
}

BufferBase::~BufferBase()
{
  // Free column name
  if (column) delete [] column;
  // Free buffers
  if (data) delete [] data;
  // Delete next
  if (next) delete next;
}

void BufferBase::init_current_buffer()
{
  LPDWORD lpdwStatus = (LPDWORD)status;
  lpdwStatus[broker.buffer_offset] = DWORD(-3);
}


Broker::Broker(const char* table_name)
: status(0)
, buffer_length(default_buffer_length)
, buffer_offset(0)
, connection_id(0)
{
	// Copy table name
  if (table_name) {
    table = new char[strlen(table_name)+1];
    strcpy(table, table_name);
  }
  else {
    table = 0;
  }
  // No buffers yet
  first = last = 0;
}

Broker::~Broker()
{
	// Free table name
  if (table) delete [] table;
  // Free buffers
	if (first) delete first;
	// Delete cursor
}

int Broker::ParseExecute(Cursor* cursor, const char* SQL, buffer_define_flags define_mode, buffer_bind_flags bind_mode, int use_binds)
{
	// Test for cursor
	if (!cursor) {
		throw DBException("No cursor!");
		// mboxstream("Broker", 0, MB_OK | MB_TASKMODAL | MB_ICONSTOP) << "No cursor!" << show;
		return 0;
	} 
//// Show bufsizes?
//if (show_bufsizes && !user_skip()) {
//   mboxstream x("Buffers");
//   for (BufferBase* mb = first; mb; mb = mb->next) {
//		x << (mb->column && mb->column[0] ? mb->column : "<anonym>") << " [" << mb->sz << ']' << endl;
//   }
//	x << "\r\n\n(hold [Ctrl] nede for at undg� disse beskeder)" << show;
// }
// // Show SQL?
//if (show_sql && !user_skip()) {
// 	DisplaySQL(SQL);
//}
  // Be optimistic! :-)
  int ok = TRUE;
  if (!cursor->Parse(SQL)) ok = FALSE;
	if (ok && define_mode && !DefineBuffers(cursor)) ok = FALSE;
	if (ok && bind_mode != BUFBIND_NONE && !BindBuffers(cursor, bind_mode)) ok = FALSE;
	if (ok && use_binds && !BindVariables(cursor, SQL)) ok = FALSE;
	if (ok) ok = cursor->Execute();
  if (!ok) {
/*
    // Special errors
    if (cursor->IsLocked()) {
      Status |= islocked;
      return 0;
    } 
*/
    // Normal errors
		//return !ReportError(cursor, SQL);
    ReportError(cursor,SQL);
  }
  return cursor->IsOK();
}


int Broker::ParseExecuteBatch(Cursor* cursor, const char* SQL, buffer_bind_flags bind_mode, int use_binds)
{
	// Test for cursor
	if (!cursor) {
		throw DBException("No cursor!");
		// mboxstream("Broker", 0, MB_OK | MB_TASKMODAL | MB_ICONSTOP) << "No cursor!" << show;
		return 0;
	} 
//// Show bufsizes?
//if (show_bufsizes && !user_skip()) {
//   mboxstream x("Buffers");
//   for (BufferBase* mb = first; mb; mb = mb->next) {
//		x << (mb->column && mb->column[0] ? mb->column : "<anonym>") << " [" << mb->sz << ']' << endl;
//   }
//	x << "\r\n\n(hold [Ctrl] nede for at undg� disse beskeder)" << show;
// }
// // Show SQL?
//if (show_sql && !user_skip()) {
//	DisplaySQL(SQL);
// }
	// Be optimistic! :-)
  int ok = TRUE;
  if (!cursor->Parse(SQL)) ok = FALSE;
	if (ok && bind_mode != BUFBIND_NONE && !BindBuffers(cursor, bind_mode)) ok = FALSE;
	if (ok && use_binds && !BindVariables(cursor, SQL)) ok = FALSE;
	if (ok) {
		// Start at the beginning
		int processed = 0;
		buffer_offset = 0;

		// Get the first thing and/or row
		const void* thing = GetThing(0L);
		int row = GetRow(0L);
		// Remember if we ever had any thing and/or row
		const int had_thing = (thing != 0);
		const int had_row = (row != 0);
		// While there are objects and/or rows to process...
		while (ok && (thing || row)) {
			// Transfer data from thing to buffers
			for (BufferBase* mb = first; mb; mb = mb->next) {
				mb->transfer_data((void*)thing, FALSE);
        mb->init_current_buffer();
			}
			// That was one more
			buffer_offset++;
			processed++;
			// Are the buffers filled?
			if (buffer_offset == buffer_length) {
				// Yup, do the duck!
        ok = buffer_length > 1 ? cursor->ExecuteArray(buffer_offset) : cursor->Execute();
				// Reset counters
				buffer_offset = 0;
			}
      // Proceed if ok
      if (ok) {
			  // Get next thing
			  if (had_thing) thing = GetThing(processed);
			  // If we had a thing, but don't have any now then we
			  // will ignore the next row
			  if (had_row) row = (!thing && had_thing) ? 0 : GetRow(processed);
      }
		}
		// Do we need to flush the buffers?
		if (buffer_offset && ok) {
			// Yup, do the duck!
      ok = buffer_length > 1 ? cursor->ExecuteArray(buffer_offset) : cursor->Execute();
			// Reset counters
			buffer_offset = 0;
		}
	}
	if (!ok) ReportError(cursor, SQL);
  return cursor->IsOK();
}

int Broker::Execute(const char* SQL)
{
	CursorHandler cursor(this);
	status = 0;
	return ParseExecute(cursor, SQL, BUFDEF_NONE, BUFBIND_NONE, TRUE);
}

int Broker::ExecuteBatch(const char* SQL)
{
	CursorHandler cursor(this);
	status = 0;
	return ParseExecuteBatch(cursor, SQL, BUFBIND_NONE, TRUE);
}


int Broker::Select(const char* SQL)
{
	CursorHandler cursor(this);
	status = 0;
  // Init buffer positions
  int pos = 1;
  for (BufferBase* mb = first; mb; mb = mb->next) {
		// Skip bind buffers
		if (mb->is_bind()) continue;
    // Assign position to buffer
    mb->pos = pos++;
  }
  // Do the duck
	if (!ParseExecute(cursor, SQL, BUFDEF_ALL, BUFBIND_NONE, TRUE)) return FALSE;
	long count = 0L;
  while (cursor->FetchArray(buffer_length)) {
    const int n = cursor->RowsFetched();
    for (buffer_offset=0; buffer_offset<n; buffer_offset++) {
      // Transfer data from internal to brokers' buffers
      for (BufferBase* mb = first; mb; mb = mb->next) {
/*
        // Check for null
        unsigned short& indicator = mb->nullbuf[buffer_offset];
        if (indicator) {
#ifdef _DEBUG
          char msg[128];
          ostrstream(msg, sizeof(msg)) 
            << "Error " << indicator << " in row "
            << (count + buffer_offset)
            << " of buffer "
            << (mb->column && mb->column[0] ? mb->column : "<unknown>")
            << "\n"
            << ends;
          OutputDebugString(msg);
#endif
          size_t offset = buffer_offset * mb->sz;
          memset(mb->buffer + offset, 0, mb->sz);
          indicator = 0;
        }
*/
		    // Don't transfer bind-vars
		    if (!mb->is_bind())
		      mb->transfer_data(0, TRUE);
	    }
      // Create new object
      void* thing = CreateThing();
      if (thing) {
        // Transfer data from buffers to thing
        for (BufferBase* mb = first; mb; mb = mb->next) {
					if (!mb->is_bind())
						mb->transfer_data(thing, TRUE);
				}
        // Handle new object
        HandleThing(thing);
			}
			else {
        // Broker does transfer data via DataBuffers
        CreateIt();
      }
    }
    // If we fetched less than a full buffer_length
    if (n < buffer_length) 
      // ... then we're done
      break;
    else
      // ... else we increase the count
      count += n;
  };
  // Restore buffer_offset
  buffer_offset=0;
	return TRUE;
}

int Broker::DefineBuffers(Cursor* cursor) const
{
	for (BufferBase* mb = first; mb; mb = mb->next) {
		// Skip buffers that are bind variables
		if (mb->is_bind()) continue;
		// Declare buffer in select list
		if (!cursor->BindColumn(mb->pos, mb->data, mb->sz, mb->type, mb->status)) {
			TRACE_ERROR("cursor->BindColumn() in DefineBuffers");
			return FALSE;
    }
	}
  return TRUE;
}

int Broker::BindBuffers(Cursor* cursor, buffer_bind_flags bind_mode) const
{
  for (BufferBase* mb = first; mb; mb = mb->next) {
		// Skip buffers that are bind variables
		if (mb->is_bind()) continue;
		// If keys only then skip buffers that are not keys
		if (bind_mode == BUFBIND_KEYS && !mb->is_key())
      continue;
		// Bind by position
		if (!cursor->BindBuffer(mb->pos, mb->data, mb->sz, mb->type, mb->status)) {
			TRACE_ERROR("cursor->BindBuffer() in BindBuffers");
			return FALSE;
    }
  }
  return TRUE;
}

int Broker::BindVariables(Cursor* cursor, const char* SQL) const
{
/*
	for (BufferBase* mb = first; mb; mb = mb->next) {
		// If this is a bind variable
		if (mb->column && mb->is_bind()) {
			// Use only bind variables actually used in the statement
			if (strstr(SQL, mb->column)) {
				// Bind by name
				if (DBI_BindByName(cursor, mb->column, mb->buffer, mb->sz, mb->type)) {
					TRACE_ERROR("DBI_BindByName() in BindVariabels");
					return FALSE;
				}
				mb->transfer_data(0, 0);
			}
		}
	}
  return TRUE;
*/
  TRACE_ERROR("BindVariables is NOT IMPLEMENTED");
  return TRUE;
}

int Broker::AutoSelect(const char* where_clause, const char* order_by, int lock)
{
	if (!table) return FALSE;
	status = 0;
	textstream SQL;
	// Build SELECT list
  SQL << "SELECT ";
  for (BufferBase* mb = first; mb; mb = mb->next) {
		// All columns must have names!
		if (!mb->column) return FALSE;
		// Skip bind buffers
		if (mb->is_bind()) continue;
		// Add column name to select list
		SQL << mb->column << ", ";
	}
	// Erase the last comma
	SQL.seekp(-2, ios::cur);
	// Build FROM list
  SQL << " FROM " << table;
  if (where_clause)
    SQL << " WHERE " << where_clause;
	if (order_by)
    SQL << " ORDER BY " << order_by;
  if (lock) {
    SQL << " FOR UPDATE OF ";
    for (BufferBase* mb = first; mb; mb = mb->next) {
			// Skip bind and key buffers
			if (mb->is_key() || mb->is_bind()) continue;
			// Add column name to update list
			SQL << mb->column << ", ";
    }
		// Erase the last comma
		SQL.seekp(-2, ios::cur) << " NOWAIT";
	}
  SQL << ends;
  // K�r svinet
  return Select(SQL);
}

int Broker::TransferToBuffers(const void* thing) 
{
	int pos = 1;
  for (BufferBase* mb = first; mb; mb = mb->next) {
		// Skip bind variables
		if (mb->is_bind()) continue;
		// Init position
		mb->pos = pos++;
		// Transfer values from thing to buffer
		mb->transfer_data((void*)thing, FALSE);
    mb->init_current_buffer();
	}
	return TRUE;
}

int Broker::ConstructInsert(textstream& SQL, const void* thing, int do_transfer)
{
	SQL << "INSERT INTO " << table << '(';
  for (BufferBase* mb = first; mb; mb = mb->next) {
		// All columns must have names!
		if (!mb->column) return FALSE;
		// Skip bind buffers
		if (mb->is_bind()) continue;
		// Add column name to list
		SQL << mb->column << ", ";
  }
	// Erase the last comma
	SQL.seekp(-2, ios::cur) << ") VALUES (";
  int pos = 1; 
	for (BufferBase* mb = first; mb; mb = mb->next) {
		// Skip bind variables
		if (mb->is_bind()) continue;
		// Bind by number
    mb->pos = pos++;
    // ORACLE: SQL << ':' << mb->pos << ", ";
    SQL << "?, ";
		// Transfer values from thing to buffer
		if (do_transfer) {
			mb->transfer_data((void*)thing, FALSE);
      mb->init_current_buffer();
    }
	}
	// Erase the last comma and close the list
	SQL.seekp(-2, ios::cur).put(')').put('\0');
	// OK, done!
  return TRUE;
}

int Broker::InsertThing(const void* thing)
{
	if (!table) return FALSE;
	CursorHandler cursor(this);
	status = 0;
	textstream SQL;
	if (ConstructInsert(SQL, thing, TRUE))
		return ParseExecute(cursor, SQL, BUFDEF_NONE, BUFBIND_ALL, FALSE);
	else
		return FALSE;
}

int Broker::InsertBatch()
{
  ScopeTempValue<int> stv(buffer_length, 1);
	if (!table) return FALSE;
	CursorHandler cursor(this);
	status = 0;
	textstream SQL;
	if (ConstructInsert(SQL))
		return ParseExecuteBatch(cursor, SQL, BUFBIND_ALL, FALSE);
	else
		return FALSE;
}


int Broker::ConstructUpdate(textstream& SQL, const void* thing, int do_transfer)
{
	SQL << "UPDATE " << table << " SET ";
  int pos = 1;
  // Construct 'SET' part
	for (BufferBase* mb = first; mb; mb = mb->next) {
		// All columns must have names!
		if (!mb->column) return FALSE;
		// Skip bind buffers
		if (mb->is_bind()) continue;
    // Skip key buffers (they go into the WHERE clause)
    if (mb->is_key()) continue;
    // Bind by number
    mb->pos = pos++;
    // ORACLE: SQL << mb->column << " = :" << mb->pos << ", ";
    SQL << mb->column << " = ?, ";
    // Transfer values to buffer
		if (do_transfer) {
			mb->transfer_data((void*)thing, FALSE);
      mb->init_current_buffer();
    }
  }
	// Delete the last comma
  SQL.seekp(-2, ios::cur);
  // Construct the WHERE clause
  SQL << " WHERE ";
	for (BufferBase* mb = first; mb; mb = mb->next) {
		// All columns must have names!
		if (!mb->column) return FALSE;
		// Skip bind buffers
		if (mb->is_bind()) continue;
    // Skip non-key buffers
    if (!mb->is_key()) continue;
    // Bind by number
    mb->pos = pos++;
    // ORACLE: SQL << mb->column << " = :" << pos << " AND ";
    SQL << mb->column << " = ? AND ";
    // Transfer values to buffer
		if (do_transfer) {
			mb->transfer_data((void*)thing, FALSE);
      mb->init_current_buffer();
    }
  }
  // Delete last 'AND'
  SQL.seekp(-5, ios::cur).put('\0');
	// OK, done!
	return TRUE;
}

int Broker::UpdateThing(const void* thing)
{
	if (!table) return FALSE;
	CursorHandler cursor(this);
	status = 0;
	textstream SQL;
	if (ConstructUpdate(SQL, thing, TRUE))
		return ParseExecute(cursor, SQL, BUFDEF_NONE, BUFBIND_ALL, FALSE);
	else
		return FALSE;
}

int Broker::UpdateBatch()
{
  ScopeTempValue<int> stv(buffer_length, 1);
	if (!table) return FALSE;
	CursorHandler cursor(this);
	status = 0;
	textstream SQL;
	if (ConstructUpdate(SQL))
		return ParseExecuteBatch(cursor, SQL, BUFBIND_ALL, FALSE);
	else
		return FALSE;
}

int Broker::ConstructDelete(textstream& SQL, const void* thing, int do_transfer)
{
	SQL << "DELETE FROM " << table << " WHERE ";
  int pos = 1;
	for (BufferBase* mb = first; mb; mb = mb->next) {
		// All columns must have names!
		if (!mb->column) return FALSE;
		// Skip bind buffers
		if (mb->is_bind()) continue;
		// Build where clause from keys
		if (mb->is_key()) {
      // Bind by number
      mb->pos = pos++;
      //SQL << mb->column << " = :" << mb->pos << " AND ";
      SQL << mb->column << " = ? AND ";
  		if (do_transfer) {
	  		mb->transfer_data((void*)thing, FALSE);
        mb->init_current_buffer();
      }
    }
  }
  // Delete last 'AND'
  SQL.seekp(-5, ios::cur).put('\0');
	// OK, done!
	return TRUE;
}

int Broker::DeleteThing(const void* thing)
{
	if (!table) return FALSE;
	CursorHandler cursor(this);
	status = 0;
	textstream SQL;
	if (ConstructDelete(SQL, thing, TRUE))
		return ParseExecute(cursor, SQL, BUFDEF_NONE, BUFBIND_KEYS, FALSE);
  else
		return FALSE;
}

int Broker::DeleteBatch()
{
  ScopeTempValue<int> stv(buffer_length, 1);
	if (!table) return FALSE;
	CursorHandler cursor(this);
	status = 0;
	textstream SQL;
	if (ConstructDelete(SQL))
		return ParseExecuteBatch(cursor, SQL, BUFBIND_KEYS, FALSE);
	else
		return FALSE;
}

int Broker::Error(int code, const char* /* SQL */) { return code == 100 ? 0 : code; }

int Broker::ReportError(Cursor* cursor, const char* SQL)
{
	if (Error(cursor->GetErrorCode(), SQL)) {
    // Normal errors
		char msg[1024]; GetError(cursor, msg, sizeof(msg));
		char msg_buf[1024]; lstrcpyn(msg_buf, SQL, sizeof(msg_buf));
    msg_buf[sizeof(msg_buf)-1] = 0;
    // Break lines at 'where' and 'from'
    for (char* s = strstr(AnsiUpper(msg_buf), " WHERE "); s; s = strstr(msg_buf, " WHERE "))
      *s = '\n';
    for (char* s = strstr(AnsiUpper(msg_buf), " FROM "); s; s = strstr(msg_buf, " FROM "))
      *s = '\n';
		throw DBException(msg);
/*
		// Build message box
		mboxstream err(IDST_ORACLE_ERROR, 0, MB_TASKMODAL | MB_ICONSTOP | (paste_error ? MB_YESNO : MB_OK));
    err << msg << endl << msg_buf;
		// Debug mode?
    if (paste_error) {
      err << "\n\nPaste message to clipboard?";
      if (err.Answer() == IDYES) {
				if (OpenClipboard(GetFocus()) && EmptyClipboard()) {
          HGLOBAL hSQL = GlobalAlloc(GPTR, lstrlen(SQL)+1);
          if (hSQL) {
            LPSTR lpSQL = (LPSTR)GlobalLock(hSQL);
            if (lpSQL) {
              lstrcpy(lpSQL, SQL);
              GlobalUnlock(hSQL);
              if (SetClipboardData(CF_TEXT, hSQL) && CloseClipboard())
                return 1;
						}
						else
							GlobalFree(hSQL);
          }
        }
			}
			else
        return 1;
//failure:
			mboxstream("Broker", 0, MB_TASKMODAL | MB_OK | MB_ICONSTOP)
        << "Can not paste to clipboard!" << show;
		}
		else {
			// Show message
      err << show;
		}
*/
		// An error ocurred
    return 1;
	}
  // No errors
	return 0;
}
