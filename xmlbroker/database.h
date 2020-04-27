// Projekt.......: BROKER
// Navn..........: database.h
// Ansvarlig.....: PH

// $Header:$

#pragma once
#include <exception>

class DBException : public std::exception {
public:
	DBException(const char* message) : msg(message) {}
    virtual const char* what() const noexcept { return msg; }
private:
	const char* const msg;
};

class Cursor {
public:
  virtual ~Cursor() = 0;
  // Status
  virtual int GetErrorCode() const = 0;
  virtual int GetErrorText(char* buffer, int buflen) const = 0;
  virtual int IsOK() const = 0;
  virtual int IsLocked() const = 0;
  virtual int RowsFetched() const = 0;
  // SQL
  virtual int Parse(const char* sql) = 0;
  virtual int Execute() = 0;
  virtual int ExecuteArray(int n) = 0;
  virtual int Fetch() = 0;
  virtual int FetchArray(int n) = 0;
  // Buffers
  virtual int BindColumn(int col, void* buffer, int buflen, int datatype, void* indicator) = 0;
  virtual int BindBuffer(int pos, void* buffer, int buflen, int datatype, void* indicator) = 0;
};

class Connection {
public:
  virtual ~Connection() = 0;
  virtual int OpenDSN(const char* username, const char* password, const char* DSN) = 0;
  virtual int OpenPath(const char* username, const char* password, const char* DSN, const char* filepath) = 0;
  virtual int Close() = 0;
  virtual Cursor* GetCursor() = 0;
};

