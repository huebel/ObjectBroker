#include "../src/ObjectBroker.h"
#include <list>
#include <iostream>

struct Test {
	int	id;
	double dbl;
};

class TestBroker : public ObjectBroker<Test> {
public:
	TestBroker() : ObjectBroker<Test>("test")
	{
		MapKeyMember(&Test::id, "id");
		MapMember(&Test::dbl, "double");
	}

	virtual Test* CreateObject() { return new Test(); }

	virtual void HandleObject(Test& t) { all.push_back(t); }


	std::list<Test> all;
};


int main(int argc, char* argv[])
{
	try {

	OpenDataBase("test.db");

	TestBroker tb;

	tb.CreateTable();

	for (int i=0; i<5; ++i) {

		Test t0 { i*42, i*3.1415926 };
		tb.Insert(t0);

	}

	tb.Select();

	for (auto t: tb.all) {
		std::cout << t.id << "," << t.dbl << std::endl;
	}

	for (int i=0; i<5; ++i) {

		if ((i&1)==0) {

			Test t0 { i*42 };
			tb.Delete(t0);

		}

	}

	tb.Select();

	for (auto t: tb.all) {
		std::cout << t.id << "," << t.dbl << std::endl;
	}

	}

	catch (BrokerException& e) {
		std::cerr << e << std::endl;
	}
	return 0;
}
