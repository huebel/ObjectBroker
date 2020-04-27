// Projekt.......: XMLBROKER
// Navn..........: xmlbroker.cpp
// Ansvarlig.....: PH



//#undef NDEBUG
//#define _NETROPY_DUMP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "xmlbroker.h"

#include <ostream>
#include <cassert>
#include <vector>

using namespace std;

template<>
XMLBroker<std::string>::XMLBroker(strxtrs::stack& context, const std::string& table_name, const std::string& rowtag)
: std::string(table_name.begin(), table_name.end()),
	Broker(c_str()),
  context(context),
  rowtag(rowtag)
{}


template<>
int XMLBroker<std::string>::Execute(const char* SQL, strxtrs::stack &data) {
	CursorHandler cursor(this);
	status = 0;
	TransferToBuffers(&data);
	return ParseExecute(cursor, SQL, BUFDEF_NONE, BUFBIND_ALL, FALSE);
}

template<>
void* XMLBroker<std::string>::CreateThing() {
  attributes.clear();
  elements.clear();
  return this;
}

template<>
inline void XMLBroker<std::string>::AddAttribute(const std::string& tag, const char* value) {
  attributes.push_back(make_pair(tag, value));
}

template<>
inline void XMLBroker<std::string>::AddElement(const std::string& tag, const char* value) {
  elements.push_back(make_pair(tag, value));
}

static inline std::string normalise(const std::string& str) {
	std::string norm(str);
	for (std::string::size_type pos = norm.find_first_of("&<>"); pos != std::string::npos; pos = norm.find_first_of("&<>", pos)) {
		switch (norm[pos]) {
		case '&':
			norm.insert(++pos, "amp;");
			pos += 4;
			break;
		case '>':
			norm[pos] = '&';
			norm.insert(++pos, "gt;");
			pos += 3;
			break;
		case '<':
			norm[pos] = '&';
			norm.insert(++pos, "lt;");
			pos += 3;
			break;
		}
	}
	return std::string(norm.begin(), norm.end());
}

template<>
inline void XMLBroker<std::string>::WriteAttribute(strxtrs::stack& row, const std::string& name, const std::string& value) {
	row
		.attr(strxtrs::attr_node(name), strxtrs::value_node(normalise(value)));
}

template<>
inline void XMLBroker<std::string>::WriteElement(strxtrs::stack& row, const std::string& tag, const std::string& value) {
	if (tag.length() > 0) {
		row
			.child(strxtrs::element_node(tag))
			.child(strxtrs::text_node(normalise(value)));
	}
	else {
		row
			.child(strxtrs::text_node(normalise(value)));
	}
}

template<>
void XMLBroker<std::string>::HandleThing(void* /* thing */) {
	strxtrs::stack& row = context.child(strxtrs::element_node(rowtag));
  for (tagmap::const_iterator a = attributes.begin(); a!=attributes.end(); ++a) {
    WriteAttribute(row, (*a).first, (*a).second);
  }
  for (tagmap::const_iterator e = elements.begin(); e!=elements.end(); ++e) {
    WriteElement(row, (*e).first, (*e).second);
  }
}

char STRING_DATA_TYPE[] = "string";

template<>
XMLBuffer<std::string>::XMLBuffer(XMLBroker<std::string>& broker, const std::string& column_name, const std::string& tag, int buffer_usage)
:   std::string(column_name.begin(), column_name.end()),
	BufferBase(broker, c_str(), 128, GetDataTypes()(STRING_DATA_TYPE), buffer_usage),
    tag(tag)
{}

template<>
void XMLBuffer<std::string>::transfer_data(void* target, int from_buffer)
{
  if (target) {
    if (from_buffer) {
			XMLBroker<std::string>& broker = *(XMLBroker<std::string>*)target;
			if (tag.size() > 0 && tag[0] == L'@') 
				broker.AddAttribute(tag.substr(1), current_buffer());
      else
        broker.AddElement(tag, current_buffer());
    } else {
			const strxtrs::stack& data = *static_cast<strxtrs::stack*>(target);
			const std::string field(tag.begin(), tag.end());
			strxtrs::list fields(data);
			while (!fields.nil()) {
				const strxtrs::xml_node& node = fields.car().info();
				switch (node.type) {
				case strxtrs::xml_node::ELM:
					if (node.content()==field) {
						strxtrs::list text(fields.car());
						if (text.nil()) {
							// TODO: NULL value
						}
						else {
							assert(text.car().info().type == strxtrs::xml_node::TXT);
							std::string value(text.car().info().content().begin(), text.car().info().content().end());
							strncpy(current_buffer(), value.c_str(), sz);
						}
					}
					break;
				default:
				    break;
				}
				fields = fields.cdr();
			}
    }
  }
}

template<>
XMLBuffer<std::string>& MapElement(XMLBroker<std::string>& broker, const std::string& column_name, const std::string& tag)
{
    TRACE("MapElement\n");
	return *new XMLBuffer<std::string>(broker, column_name, tag, BufferBase::use_select);
}

template<>
XMLBuffer<std::string>& MapText(XMLBroker<std::string>& broker)
{
    TRACE("MapText\n");
	return *new XMLBuffer<std::string>(broker, "", "", BufferBase::use_select);
}
