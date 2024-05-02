#include "socket.hh"
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

using namespace std;

void get_URL( const string& host, const string& path )
{
  // Update here with your code! Don't forget to remove the next two lines
  const Address webpageaddr( host, "http" );
  TCPSocket sock;
  sock.connect( webpageaddr );
  const string clientrequest = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection:  close\r\n\r\n";
  sock.write( clientrequest );
  string holder;
  bool flag = false;
  while ( !flag ) {
    sock.read( holder );
    if ( holder.empty() ) {
      flag = true;
    } else {
      cout << holder;
    }
  }
  sock.close();
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " api.ipify.org /\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
