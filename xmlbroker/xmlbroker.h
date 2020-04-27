#ifndef __XMLBROKER_H
#define __XMLBROKER_H

#include "broker_impl.h"
#include "broker.h"

#include <ostream>
#include <vector>

#include <strxtrs/stack.h>

template <typename Range>
class XMLBuffer;

template <typename Range>
class XMLBroker : private std::string, public Broker {
public:
	XMLBroker(strxtrs::stack& context, const Range& table_name, const Range& rowtag);
	int Execute(const char* SQL, strxtrs::stack& data);
protected:
  virtual void* CreateThing();
  virtual void HandleThing(void* /* thing */);
	virtual const void* GetThing(long /* idx */) { return 0; }
	virtual int GetRow(long /*idx*/) { return 0; }
private:
  strxtrs::stack& context;
	const Range& rowtag;
  // XML helper functions
  void AddAttribute(const Range& tag, const char* value);
  void AddElement(const Range& tag, const char* value);
	void WriteAttribute(strxtrs::stack& row, const Range& name, const std::string& value);
	void WriteElement(strxtrs::stack& row, const Range& tag, const std::string& value);
	typedef std::vector<std::pair<Range, std::string> > tagmap;
  tagmap attributes;
  tagmap elements;
friend
  class XMLBuffer<Range>;
};

template <typename Range>
class XMLBuffer : private std::string, public BufferBase {
public:
	XMLBuffer(XMLBroker<Range>& broker, const Range& column_name, const Range& tag, int buffer_usage);
private:
  // Transfer values
  virtual void transfer_data(void* target, int from_buffer);
  // The buffer tag
  const Range tag;
};

template <typename Range>
XMLBuffer<Range>& MapElement(XMLBroker<Range>& broker, const Range& column_name, const Range& tag);

template <typename Range>
XMLBuffer<Range>& MapText(XMLBroker<Range>& broker);


#endif//__XMLBROKER_H
