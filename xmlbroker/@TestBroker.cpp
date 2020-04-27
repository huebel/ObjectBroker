#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "database.h"
#include "ODBCConnection.h"
#include "xmlbroker.h"

#include <iostream>
#include <fstream>


void execute_query(std::ostream& out) {
  Connection* c = GetODBCConnection();
  c->OpenDSN("", "", "Netropy Test");
  
  XMLBroker<std::wstring> b(out, L"centerinfo", L"center");
  MapElement(b, "afdnr", "@afdnr");
  MapElement(b, "afdeling");
  MapElement(b, "postnr");
  MapElement(b, "by");
  b.AutoSelect();
//  int param=200;
//  BindBuffer(b, &param, "@param");
//  b.AutoSelect("afdnr < @param");
}


int main(int argc, char* argv[])
{
  using namespace std;

  ifstream in("center.xml");



  execute_query(cout);

  cout << "Press <Enter> to close..." << endl;
  char s; cin.getline(&s,1);;
	return 0;
}
