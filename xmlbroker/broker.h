// Projekt.......: BROKER
// Navn..........: broker.h
// Ansvarlig.....: TN/PH

// $Header:$

#ifndef __BROKER_H
#define __BROKER_H

#include "datatype.h"

#include <cstddef>

/* Broker klassen */

#ifndef BROKER_PRIVATE_IMPLEMTATION_FUNCTIONS
#define BROKER_PRIVATE_IMPLEMTATION_FUNCTIONS
#endif

class BufferBase;

class Broker {
  friend class BufferBase;
public:
  // Status bits & constants
  enum broker_status {
    notfound = 1, islocked = 2, default_buffer_length = 20
  };
  // ArraySize
  void SetBufferSize(int length) {
    buffer_length = length;
  }
  // Set connection
  void SetConnection(int id) {
    connection_id = id;
  }
  // General SQL interface
  int Execute(const char* SQL);
  int ExecuteBatch(const char* SQL);
  int Select(const char* SQL);
  // Use auto-generated statements
  int AutoSelect(const char* where_clause = 0, const char* order_by = 0, int lock = 0);
  // Status
  int status;
  // Error handling
  virtual int Error(int code, const char* message);
  virtual void LockConflict() {
  }
protected:
  Broker(const char* table_name);
  virtual ~Broker();
  virtual void CreateIt() {}
  virtual void* CreateThing() = 0;
  virtual void HandleThing(void* thing) = 0;
  int InsertThing(const void* thing);
  int UpdateThing(const void* thing);
  int DeleteThing(const void* thing);
  // Batch processing
  int InsertBatch();
  int UpdateBatch();
  int DeleteBatch();
  virtual const void* GetThing(long idx) = 0;
  virtual int GetRow(long idx) = 0;
private:
  BROKER_PRIVATE_IMPLEMTATION_FUNCTIONS
  // Buffers
  BufferBase *first, *last;
  // DB info
  char* table;
  int buffer_length;
  int buffer_offset;
  // Connection
  int connection_id;
};

template<class T>
class ObjectBroker: public Broker {
public:
  ObjectBroker(const char* table_name = 0)
  : Broker(table_name)
  , current_object(nullptr)
  {
  }
  // Insert/update/delete needs an object
  int AutoInsert(const T & object) {
    return InsertThing(current_object = &object);
  }
  int AutoUpdate(const T & object) {
    return UpdateThing(current_object = &object);
  }
  int AutoDelete(const T & object) {
    return DeleteThing(current_object = &object);
  }
  // Batch versions
  int AutoInsert() {
    return InsertBatch();
  }
  int AutoUpdate() {
    return UpdateBatch();
  }
  int AutoDelete() {
    return DeleteBatch();
  }
  // Object handling
  virtual T* CreateObject() {
    return 0;
  }
  virtual void HandleObject(T & /*object*/) {
  }
  virtual const T* GetObject(long idx) {
    return 0;
  }
protected:
  typedef ObjectBroker<T> Super;
  const T* current_object;
private:
  virtual void* CreateThing() {
    return CreateObject();
  }
  virtual void HandleThing(void* thing) {
    HandleObject(*(T*) thing);
  }
  virtual const void* GetThing(long idx) {
    return current_object = GetObject(idx);
  }
  virtual int GetRow(long /*idx*/) {
    return 0;
  }
};

class SQLBroker: public Broker {
public:
  SQLBroker(const char* table_name = 0)
  : Broker(table_name)
  {}
protected:
  virtual void* CreateThing() {
    return 0;
  }
  virtual void HandleThing(void* /* thing */) {
  }
  virtual const void* GetThing(long /* idx */) {
    return 0;
  }
  virtual int GetRow(long /*idx*/) {
    return 0;
  }
};

class BufferBase {
public:
  // Allocation
  void* operator new(size_t);
  void operator delete(void*);
  // Usage types
  enum use_buffer {
    use_select = 0x0000, use_key = 0x0001, use_bind = 0x0002
  };
protected:
  int is_select() {
    return usage == use_select;
  }
  int is_key() {
    return usage & use_key;
  }
  int is_bind() {
    return usage & use_bind;
  }
  // Lifetime
  BufferBase(Broker& broker, const char* column_name, size_t size, int type_code, int is_key);
  virtual ~BufferBase();
  // Transfer
  virtual void transfer_data(void* thing, int from_buffer) = 0;
  // Owner
  Broker& broker;
  // Buffer info
  char* data;
  char* status;
  const size_t sz;
  BufferBase *next;
  // DB info
  char* column;
  const int type;
  const int usage;
  // Bind position
  int pos;
  // current buffer
  void init_current_buffer();
  char* current_buffer() {
    return data + sz * (broker.buffer_offset);
  }
  friend class Broker;
};

template<class S> // Member type
class BufferHandler {
public:
  size_t buffer_size() {
    return sizeof(S);
  }
  int type_code() {
    return GetDataTypes()(*(S*) 0);
  }
  void buffer_to_member(S* addr, char* buffer) {
    memcpy(addr, buffer, sizeof(S));
  }
  void member_to_buffer(S* addr, char* buffer) {
    memcpy(buffer, addr, sizeof(S));
  }
};

/*
 class BufferHandler<SpecialCase> {
 public:
 size_t buffer_size();
 int type_code();
 void buffer_to_member(SpecialCase* addr, char* buffer);
 void member_to_buffer(SpecialCase* addr, char* buffer);
 };
 */

// MEMBER BUFFER
template<class T, class S> // Object type, member type
class MemberBuffer: private BufferBase, public BufferHandler<S> {
public:
  MemberBuffer(Broker& broker, const char* column_name, S T::*member, int buffer_usage)
  : BufferBase(broker, column_name, BufferHandler<S>::buffer_size(), BufferHandler<S>::type_code(), buffer_usage)
  , ptm(member)
  {}
private:
  // Transfer values
  virtual void transfer_data(void* thing, int from_buffer) {
    if (thing) {
      T* object = (T*) thing;
      if (from_buffer)
        buffer_to_member(&(object->*ptm), current_buffer());
      else
        member_to_buffer(&(object->*ptm), current_buffer());
    }
  }
  // The member
  S T::*ptm;
};

template<class T, class S>
MemberBuffer<T, S>& MapMember(S T::*member, const char* column_name) {
  return *new MemberBuffer<T, S>(column_name, member, BufferBase::use_select);
}

template<class T, class S>
MemberBuffer<T, S>& MapMember(S T::*member, char* column_name) {
  return *new MemberBuffer<T, S>(column_name, member, BufferBase::use_select);
}

template<class T, class S>
MemberBuffer<T, S>& MapKeyMember(S T::*member, const char* column_name) {
  return *new MemberBuffer<T, S>(column_name, member, BufferBase::use_key);
}

template<class T, class S>
MemberBuffer<T, S>& MapKeyMember(S T::*member, char* column_name) {
  return *new MemberBuffer<T, S>(column_name, member, BufferBase::use_key);
}

template<class T, class S>
MemberBuffer<T, S>& BindMember(S T::*member, const char* column_name) {
  return *new MemberBuffer<T, S>(column_name, member, BufferBase::use_bind);
}

template<class T, class S>
MemberBuffer<T, S>& BindMember(S T::*member, char* column_name) {
  return *new MemberBuffer<T, S>(column_name, member, BufferBase::use_bind);
}

// SIMPLE BUFFER

template<class S> // Simple type
class SimpleBuffer: public BufferBase, public BufferHandler<S> {
public:
  SimpleBuffer(Broker& broker, const char* column_name, S* var, int buffer_usage)
  : BufferBase(broker, column_name, BufferHandler<S>::buffer_size(), BufferHandler<S>::type_code(), buffer_usage)
  , bufvar(var)
  {}
private:
  // Transfer values
  virtual void transfer_data(void* /* thing */, int from_buffer) {
    if (from_buffer)
      buffer_to_member(bufvar, current_buffer());
    else
      member_to_buffer(bufvar, current_buffer());
  }
  // The buffer variable
  S* bufvar;
};

template<class S>
SimpleBuffer<S>& MapBuffer(Broker& broker, S* var, const char* column_name) {
  return *new SimpleBuffer<S>(broker, column_name, var, BufferBase::use_select);
}

template<class S>
SimpleBuffer<S>& MapBuffer(Broker& broker, S* var, char* column_name) {
  return *new SimpleBuffer<S>(broker, column_name, var, BufferBase::use_select);
}

template<class S>
SimpleBuffer<S>& MapKeyBuffer(Broker& broker, S* var, const char* column_name) {
  return *new SimpleBuffer<S>(broker, column_name, var, BufferBase::use_key);
}

template<class S>
SimpleBuffer<S>& MapKeyBuffer(Broker& broker, S* var, char* column_name) {
  return *new SimpleBuffer<S>(broker, column_name, var, BufferBase::use_key);
}

template<class S>
SimpleBuffer<S>& BindBuffer(Broker& broker, S* var, const char* column_name) {
  return *new SimpleBuffer<S>(broker, column_name, var, BufferBase::use_bind);
}

//template <class S>
//SimpleBuffer<S>& BindBuffer(Broker& broker, S* var, char* column_name)
//{
//	return *new SimpleBuffer<S>(broker, column_name, var, BufferBase::use_bind);
//}

#endif//__BROKER_H
