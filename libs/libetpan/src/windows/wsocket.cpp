/*

Special class, supposed to be held as a static object in
any class that uses the sockets api - Socket.cc, GenericSocket.cc,
NetAddress.cc

*/
class win_sockets {
  public:
	win_sockets() { 

    int success = WSAStartup((WORD)0x0101, &winsockData);
    if (success != 0)
      {
		throw "Cannot startup windows sockets api.";
      }
	}
	~win_sockets() {
		WSACleanup();
	}

  private:
    WSADATA winsockData;
};

/* Initialise sockets */
static win_sockets wsastartup;	
