#ifndef __BROKER_IMPL_H
#define __BROKER_IMPL_H

enum buffer_define_flags { BUFDEF_NONE = 0, BUFDEF_ALL = 1 };
enum buffer_bind_flags { BUFBIND_NONE = 0, BUFBIND_ALL = 1, BUFBIND_KEYS = -1 };

class textstream;
class Broker;
class Cursor;

extern Cursor* GetCursor(Broker*);
extern void ReleaseCursors(Broker*);
extern void ReleaseCursor(Cursor*);
extern int GetError(Cursor*, char* buffer, int bufsiz);

class CursorHandler {
public:
	CursorHandler(Broker* b) : c(GetCursor(b)) {}
	~CursorHandler() { ReleaseCursor(c); }
	operator Cursor*() const { return c; }
	Cursor* operator->() const { return c; }
private:
	Cursor* c;
};

#define BROKER_PRIVATE_IMPLEMTATION_FUNCTIONS \
protected: \
	int ParseExecute(Cursor* cursor, const char* SQL, buffer_define_flags define_mode, buffer_bind_flags bind_mode, int use_binds); \
	int TransferToBuffers(const void* thing); \
private: \
	int ParseExecuteBatch(Cursor* cursor, const char* SQL, buffer_bind_flags bind_mode, int use_binds); \
	int DefineBuffers(Cursor* cursor) const; \
	int BindBuffers(Cursor* cursor, buffer_bind_flags bind_mode) const; \
	int BindVariables(Cursor* cursor, const char* SQL) const; \
	int ReportError(Cursor* cursor, const char* SQL);\
\
	int ConstructInsert(textstream& SQL, const void* thing = 0, int do_transfer = 0); \
	int ConstructUpdate(textstream& SQL, const void* thing = 0, int do_transfer = 0); \
	int ConstructDelete(textstream& SQL, const void* thing = 0, int do_transfer = 0); \

#endif//__BROKER_IMPL_H