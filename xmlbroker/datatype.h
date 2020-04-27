#ifndef __SQLTYPES
#include <sqltypes.h>
#endif

class DataTypes {
public:
  virtual ~DataTypes() {}
  virtual int operator()(int&) const = 0;
  virtual int operator()(unsigned int&) const = 0;
  virtual int operator()(long&) const = 0;
  virtual int operator()(unsigned long&) const = 0;
  virtual int operator()(float&) const = 0;
  virtual int operator()(double&) const = 0;
  virtual int operator()(char&) const = 0;
  virtual int operator()(char*) const = 0;
  virtual int operator()(TIMESTAMP_STRUCT&) const = 0;
};

extern const DataTypes& GetDataTypes();
