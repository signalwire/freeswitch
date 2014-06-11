%{
#include "mcast.h"
#include "mcast_cpp.h"
%}

%newobject McastHANDLE::recv;

%include "mcast_cpp.h"

%perlcode %{
use constant {
	MCAST_SEND => (1 << 0),
	MCAST_RECV => (1 << 1),
	MCAST_TTL_HOST => (1 << 2),
	MCAST_TTL_SUBNET => (1 << 3),
	MCAST_TTL_SITE => (1 << 4),
	MCAST_TTL_REGION => (1 << 5),
	MCAST_TTL_CONTINENT => (1 << 6),
	MCAST_TTL_UNIVERSE => (1 << 7)  
};
%}



