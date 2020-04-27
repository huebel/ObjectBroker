#include "../src/ObjectBroker.h"
#include <list>

struct Test {
	int	id;
};


class TestBroker : public ObjectBroker<Test> {
public:
	TestBroker() : ObjectBroker<Test>("test")
	{
		MapMember(&Test::id, "id");
	}

	virtual Test* CreateObject() { return new Test(); }

	virtual void HandleObject(Test& t) { all.push_back(t); }


	std::list<Test> all;
};


int main(int argc, char* argv[])
{
	TestBroker tb;

	tb.CreateTable();

	return 0;
}
