#ifndef __TEXTSTREAM_H
#define __TEXTSTREAM_H

#ifndef _STRSTREAM_
#include <strstream>
#endif

#ifndef _OSTREAM_
#include <ostream>
#endif

class textstream: public std::ostream {
public:
  textstream() : std::ostream(&_buf) {}
  // Get the string (ensures zero termination and resets string)
  operator const char*() {
    put('\0');
    seekp(0);
    return _buf.pbase();
  }
  // Return the length of the string
  int length() {
    return _buf.pcount();
  } //{ return _buf.out_waiting(); }
private:
  class textstreambuf: public std::strstreambuf {
  public:
    char* pbase() {
      return strstreambuf::pbase();
    }
    //int out_waiting() { return strstreambuf::out_waiting(); }
  } _buf;
};

#endif//__TEXTSTREAM_H
