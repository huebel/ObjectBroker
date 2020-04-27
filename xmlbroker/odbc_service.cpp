//#undef NDEBUG
//#define _NETROPY_DUMP

#include <tools/trace.h>

#include <xml/xml_grammar.h>
#include <xml/xml_output.h>
#include <strxtrs/path.h>
#include <strxtrs/xml_builder.h>

#include <netropy/api/transport.h>

#include <netropy/generator.h>
#include <xmlbroker/odbc_service.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "database.h"
#include "ODBCConnection.h"
#include "xmlbroker.h"

#include <boost/lexical_cast.hpp>

#include <netropy/core/stash.h>

namespace netropy {

	static inline const strxtrs::stack& parse_attributes(
		const strxtrs::stack& data, 
		std::string& content_type,
		std::string& encoding,
		std::string& style,
		std::string& batch_tag,
		std::string& database
	) {
		strxtrs::list children(data);
		while (!children.nil() && children.car().info().type == strxtrs::xml_node::ATT) {
			const strxtrs::xml_node& attr = children.car().info();
			if (attr.content()=="type") {
				strxtrs::list val(children.car());
				content_type = val.car().info().content();
			}
			else if (attr.content()=="encoding") {
				strxtrs::list val(children.car());
				encoding = val.car().info().content();
			}
			else if (attr.content()=="style") {
				strxtrs::list val(children.car());
				style = val.car().info().content();
			}
			else if (attr.content()=="database") {
				strxtrs::list val(children.car());
				database.assign(val.car().info().content().begin(), val.car().info().content().end());
			}
			else if (attr.content()=="batch") {
				strxtrs::list val(children.car());
				batch_tag = val.car().info().content();
			}
			children = children.cdr();
		}
		while (!children.nil() && children.car().info().type != strxtrs::xml_node::ELM) {
			children = children.cdr();
		}
		if (children.nil()) 
			throw std::runtime_error("no document node??");
		else
			return children.car();
	}

	struct odbc_service : service_point<odbc_service>
	{
		odbc_service(strxtrs::stack& context);
		void enter(strxtrs::stack& context, strxtrs::path& route);
		void exit(strxtrs::stack& context) {};
	private:
		const strxtrs::stack& my_context;
		// attributes
		std::string content_type;
		std::string encoding;
		std::string style;
		std::string batch_tag;
		std::string database;
		// parsed inner structure
		const strxtrs::stack& query;
		std::string sql_string;
		// verbs
		void output(const api::transport& request, strxtrs::stack& context);
        //void input(const strxtrs::stack& request, strxtrs::stack& context);
		void input(const api::transport& request, strxtrs::stack& context);
		// helpers
		strxtrs::stack& wrap(strxtrs::stack& context);
		std::string& sql();
		const std::string& rowtag() const { return query.info().content(); }
		static const std::string& get_header(const std::string& path, const strxtrs::stack& request);
		void report_exception(strxtrs::stack& context, const std::exception& e);
		void init_select_broker(XMLBroker<std::string>& b);
		void init_execute_broker(XMLBroker<std::string>& b);
		// Thread safety
		CRITICAL_SECTION cs;
	};

	odbc_service::odbc_service(strxtrs::stack& context)
	:   my_context(context),
		content_type("text/xml"),
		encoding("utf-8"),
		query(parse_attributes(context, content_type, encoding, style, batch_tag, database)) {
		// Install this service in the context
		context.info().inject(this);	
#ifndef NDEBUG
		// query.dump("INIT odbc_service");
		TRACE("odbc_service: on database[%s] do: %s\n", database.c_str(), sql().c_str());
#endif
		// Thread safety
		InitializeCriticalSection(&cs);
	}

	void odbc_service::enter(strxtrs::stack& context, strxtrs::path& route) {
		// assume ok
		std::string status_code("200 OK");
#ifdef _NETROPY_DUMP
		context.dump("CALL odbc_service - before");
#endif
		Connection* c = 0;
		try {
			EnterCriticalSection(&cs);
			c = GetODBCConnection();
			c->OpenDSN("", "", database.c_str());
#ifdef _NETROPY_DUMP
			context.dump("request");
#endif
			api::transport request(context);
            if (request.can_deliver("application/xml")) {
                input(request, context);
            }
            else if (request.will_accept("application/xml")) {
                output(request, context);
            }

//			const strxtrs::stack& request = (context.root().begin()-1)->root();
//			const std::string& method = get_header("/http:Request/method", request);
//			if (method == "PUT") {
//				input(request, context);
//			}
//			else if (method == "GET") {
//				output(request, context);
//			}


			c->Close();
			LeaveCriticalSection(&cs);
		}		
		catch (const DBException& e) {
			if (c) c->Close();
			LeaveCriticalSection(&cs);
			status_code = "500 Internal Server Error";
			report_exception(context, e);
		}
		catch (const std::exception& e) {
			if (c) c->Close();
			LeaveCriticalSection(&cs);
			status_code = "500 Internal Server Error";
			report_exception(context, e);
		}
#ifdef _NETROPY_DUMP
		context.dump("CALL odbc_service - after");
#endif
		char length[128]; sprintf(length, "%d", xml_length(context));
		strxtrs::stack& header = context.freeze_frame(0);
		header.attr("HTTP-Version", NETROPY_HTTP_VERSION);
		header.attr("Status-Code", status_code);
		header.attr("Server", NETROPY_SERVER_VERSION);
		header.attr("Content-Length", length);
		header.attr("Content-Type", content_type);
	}

	strxtrs::stack& odbc_service::wrap(strxtrs::stack& context) {
		return batch_tag.empty() ? context : context.child(strxtrs::element_node(batch_tag));
	}
	
	const std::string& odbc_service::get_header(const std::string& path, const strxtrs::stack& request) {
		const strxtrs::stack* found = strxtrs::lookup(request, path);
		if (!found) throw std::runtime_error("bad request");
		return found->value().info().content();
	}

    void odbc_service::input(const api::transport& request, strxtrs::stack& context) {
        const ptrdiff_t expected = boost::lexical_cast<ptrdiff_t>(request.content_length);

        TRACE(">> PUT %s (%d bytes) >>\n", request.content_type.c_str(), expected);
        std::streambuf* sb = context.info().tap<std::streambuf>();
        std::string mime_type(request.content_type);

        stash* in = stash::create(sb, expected, mime_type.c_str());
        std::string xml;
        xml.reserve(in->size());
//      char* buffer = const_cast<char*>(xml.c_str());
//      int buflen = ::MultiByteToWideChar(CP_UTF8, 0, in->begin(), in->size(), buffer, in->size());
//      xml.assign(buffer, buffer+buflen);
        xml.assign(in->begin(), in->end());
        strxtrs::xml_builder builder(context);
        if (pstade::biscuit::match< xml_grammar::grammar::document >(xml, builder)) {
            stash::destroy(in);
            XMLBroker<std::string> broker(context, rowtag(), rowtag());
            init_execute_broker(broker);
            broker.Execute(sql().c_str(), strxtrs::access(context, "/"+rowtag()));
        }
        else {
            stash::destroy(in);
            throw std::runtime_error("bad XML in request");
        }
    }

	void odbc_service::output(const api::transport& request, strxtrs::stack& context) {
			//struct score {
			//	char name[128];
			//	char seconds[128];
			//};
			//score t = { ":-)", "0.343" };
			//ObjectBroker<score> btest("highscore");
			//MapBuffer(btest, &t.name, "name");
			//MapBuffer(btest, &t.seconds, "score");
			//btest.AutoInsert(t);

			std::string xml_header("<?xml version='1.0' encoding='");
			xml_header.append(encoding).append("'?>");
			// This is XML
			context.atom(strxtrs::pi_node(xml_header));

			if (!style.empty()) {
				std::string sheet("<?xml-stylesheet type='text/xsl' href='");
				sheet.append(style).append("'?>");
				context.atom(strxtrs::pi_node(sheet));
			};

			if (sql().find("SELECT") != std::string::npos) {
				XMLBroker<std::string> broker(wrap(context), rowtag(), rowtag());
				init_select_broker(broker);
				broker.Select(sql().c_str());
			}
			else {
				// Testdata
				//strxtrs::stack& score = context.child(strxtrs::element_node("score"));
				//wchar_t time[16];
				//wsprintfW(time, "%ld", ::GetTickCount());
				//score
				//	.child(strxtrs::element_node("name"))
				//	.child(strxtrs::text_node("NS :-)"));
				//score
				//	.child(strxtrs::element_node("seconds"))
				//	.child(strxtrs::text_node(time));

			}	
	}

	std::string& odbc_service::sql() {
		if (sql_string.length() < 1) {
			const strxtrs::stack* const probe = query.begin();
			for (unsigned i=0; i < query.height(); i++) {
				switch (probe[i].info().type) {
				case strxtrs::xml_node::TXT:
					sql_string.append(probe[i].info().content().begin(), probe[i].info().content().end());
					break;
				default:
				    break;
				}
			}
		}
		return sql_string;
	}

	void odbc_service::report_exception(strxtrs::stack& context, const std::exception& e) {
		std::string msg(e.what());
		std::string message(msg.begin(), msg.end());
		strxtrs::stack& error = context
			.child(strxtrs::element_node("sql:error"))
			.attr("xmlns:sql", "http://www.strxtrs.net/netropy/sql");
		error
			.child(strxtrs::element_node("sql:message"))
			.child(strxtrs::text_node(message));
		error
			.child(strxtrs::element_node("sql:service"))
			.attr("xmlns:netropy", "http://www.strxtrs.net/netropy")
			.push(my_context);
	}

	void odbc_service::init_select_broker(XMLBroker<std::string>& b) {
		strxtrs::list tags(query);
		while (!tags.nil()) {
			const strxtrs::xml_node& info = tags.car().info();
			switch (info.type) {
			case strxtrs::xml_node::ELM:
				if (info.content().compare("sql:attribute")==0) {
					strxtrs::list attr(tags.car());
					if (!attr.nil() && attr.car().info().type == strxtrs::xml_node::ATT && attr.car().info().content().compare("name")==0) {
						const strxtrs::stack& value = strxtrs::list(attr.car()).car();
						std::string tag(value.info().content());
						tag.insert(0, 1, '@');
						MapElement(b, value.info().content(), tag);
					}
				}
				else if (info.content().compare("sql:text")==0) {
					MapText(b);
				}
				else {
					std::string tag(info.content());
					MapElement(b, tag, tag);
				}
				break;
			default:
			    break;
			}
			tags = tags.cdr();
		}
	}

	void odbc_service::init_execute_broker(XMLBroker<std::string>& b) {
		strxtrs::list tags(query);
		while (!tags.nil()) {
			const strxtrs::xml_node& node = tags.car().info();
			switch (node.type) {
			case strxtrs::xml_node::ELM:
			{
				strxtrs::list text(tags.car());
				assert(text.car().info().type == strxtrs::xml_node::TXT);
				if (text.car().info().content() == "?") {
					std::string tag(node.content());
					MapElement(b, tag, tag);
				}
	            break;
			}
			default:
			    break;
			}
			tags = tags.cdr();
		}
	}

	// GENERATOR
	generator* odbc_generator() {
		static default_generator<odbc_service> generator;
		return &generator;
	}

} // namespace netropy
