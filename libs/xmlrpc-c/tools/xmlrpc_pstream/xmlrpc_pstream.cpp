#include <cassert>
#include <cerrno>
#include <string>
#include <vector>
#include <list>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <readline/readline.h>

#include "cmdline_parser.hpp"
#include "xmlrpc-c/girerr.hpp"
using girerr::throwf;

#include <features.h>  // for __BEGIN_DECLS

__BEGIN_DECLS
#include "dumpvalue.h"  /* An internal Xmlrpc-c header file ! */
__END_DECLS


#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client.hpp>
#include <xmlrpc-c/client_transport.hpp>

using namespace std;
using namespace xmlrpc_c;

/*----------------------------------------------------------------------------
   Command line
-----------------------------------------------------------------------------*/

class cmdlineInfo {
public:
    int            serverfd;
    bool           interactive;

    // Valid only if !interactive:
    string         methodName;
    vector<string> params;

    cmdlineInfo(int           const argc,
                const char ** const argv);

private:
    cmdlineInfo();
};



static void
parseCommandLine(cmdlineInfo * const cmdlineP,
                 int           const argc,
                 const char ** const argv) {

    CmdlineParser cp;

    cp.defineOption("serverfd",       CmdlineParser::UINT);

    try {
        cp.processOptions(argc, argv);
    } catch (exception const& e) {
        throwf("Command syntax error.  %s", e.what());
    }

    if (cp.optionIsPresent("serverfd")) {
        cmdlineP->serverfd = cp.getOptionValueUint("serverfd");
    } else
        cmdlineP->serverfd = 3;
    
    if (cp.argumentCount() < 1)
        cmdlineP->interactive = true;
    else {
        cmdlineP->interactive = false;
        cmdlineP->methodName = cp.getArgument(0);
        for (uint argI = 1; argI < cp.argumentCount(); ++argI)
            cmdlineP->params.push_back(cp.getArgument(argI));
    }            
}



cmdlineInfo::
cmdlineInfo(int           const argc,
            const char ** const argv) {

    try {
        parseCommandLine(this, argc, argv);
    } catch (exception const& e) {
        throwf("Command syntax error.  %s", e.what());
    }
}



static value
bytestringValFromParm(string const& valueString) {

    value retval;

    if (valueString.length() / 2 * 2 != valueString.length())
        throwf("Hexadecimal text is not an even "
               "number of characters (it is %u characters)",
               valueString.length());
    else {
        vector<unsigned char> byteString(valueString.length() / 2);
        size_t strCursor;

        strCursor = 0;

        while (strCursor < valueString.length()) {
            string const hexByte(valueString.substr(strCursor, 2));

            unsigned char byte;
            int rc;

            rc = sscanf(hexByte.c_str(), "%2hhx", &byte);

            byteString.push_back(byte);

            if (rc != 1)
                throwf("Invalid hex data '%s'", hexByte.c_str());
            else
                strCursor += 2;
        }
        retval = value_bytestring(byteString);
    }
    return retval;
}



static value
intValFromParm(string const& valueString) {

    value retval;

    if (valueString.length() < 1)
        throwf("Integer argument has nothing after the 'i/'");
    else {
        long longValue;
        char * tailptr;

        errno = 0;
        
        longValue = strtol(valueString.c_str(), &tailptr, 10);

        if (errno == ERANGE)
            throwf("'%s' is out of range for a 32 bit integer",
                   valueString.c_str());
        else if (errno != 0)
            throwf("Mysterious failure of strtol(), errno=%d (%s)",
                   errno, strerror(errno));
        else {
            if (*tailptr != '\0')
                throwf("Integer argument has non-digit crap in it: '%s'",
                       tailptr);
            else
                retval = value_int(longValue);
        }
    }
    return retval;
}



static value
boolValFromParm(string const& valueString) {

    value retval;

    if (valueString == "t" || valueString == "true")
        retval = value_boolean(true);
    else if (valueString == "f" || valueString == "false")
        retval = value_boolean(false);
    else
        throwf("Boolean argument has unrecognized value '%s'.  "
               "recognized values are 't', 'f', 'true', and 'false'.",
               valueString.c_str());

    return retval;
} 



static value
doubleValFromParm(string const& valueString) {

    value retval;

    if (valueString.length() < 1)
        throwf("\"Double\" argument has nothing after the 'd/'");
    else {
        double value;
        char * tailptr;

        value = strtod(valueString.c_str(), &tailptr);

        if (*tailptr != '\0')
            throwf("\"Double\" argument has non-decimal crap in it: '%s'",
                   tailptr);
        else
            retval = value_double(value);
    }
    return retval;
}



static value
nilValFromParm(string const& valueString) {

    value retval;

    if (valueString.length() > 0)
        throwf("Nil argument has something after the 'n/'");
    else
        retval = value_nil();

    return retval;
}



static value
i8ValFromParm(string  const& valueString) {

    value retval;

    if (valueString.length() < 1)
        throwf("Integer argument has nothing after the 'I/'");
    else {
        long long value;
        char * tailptr;
        
        errno = 0;

        value = strtoll(valueString.c_str(), &tailptr, 10);

        if (errno == ERANGE)
            throwf("'%s' is out of range for a 64 bit integer",
                   valueString.c_str());
        else if (errno != 0)
            throwf("Mysterious failure of strtoll(), errno=%d (%s)",
                   errno, strerror(errno));
        else {
            if (*tailptr != '\0')
                throwf("64 bit integer argument has non-digit crap "
                       "in it: '%s'",
                       tailptr);
            else
                retval = value_i8(value);
        }
    }
    return retval;
}



static value
parameterFromArg(string  const& paramArg) {

    value param;

    try {
        if (paramArg.substr(0, 2) == "s/")
            param = value_string(paramArg.substr(2));
        else if (paramArg.substr(0, 2) == "h/")
            param = bytestringValFromParm(paramArg.substr(2));
        else if (paramArg.substr(0, 2) == "i/")
            param = intValFromParm(paramArg.substr(2));
        else if (paramArg.substr(0, 2) == "I/")
            param = i8ValFromParm(paramArg.substr(2));
        else if (paramArg.substr(0, 2) == "d/")
            param = doubleValFromParm(paramArg.substr(2));
        else if (paramArg.substr(0, 2) == "b/")
            param = boolValFromParm(paramArg.substr(2));
        else if (paramArg.substr(0, 2) == "n/")
            param = nilValFromParm(paramArg.substr(2));
        else {
            /* It's not in normal type/value format, so we take it to be
               the shortcut string notation 
            */
            param = value_string(paramArg);
        }
    } catch (exception const& e) {
        throwf("Failed to interpret parameter argument '%s'.  %s",
               paramArg.c_str(), e.what());
    }
    return param;
}



static paramList
paramListFromParamArgs(vector<string> const& params) {
    
    paramList paramList;

    for (vector<string>::const_iterator p = params.begin();
         p != params.end(); ++p)
        paramList.add(parameterFromArg(*p));

    return paramList;
}



static void
callWithClient(client *  const  clientP,
               string    const& methodName,
               paramList const& paramList,
               value *   const  resultP) {
               
    rpcPtr myRpcP(methodName, paramList);

    carriageParm_pstream myCarriageParm;  // Empty - no parm needed

    try {
        myRpcP->call(clientP, &myCarriageParm);
    } catch (exception const& e) {
        throwf("RPC failed.  %s", e.what());
    }
    *resultP = myRpcP->getResult();
}



static void
dumpResult(value const& result) {

    cout << "Result:" << endl << endl;

    /* Here we borrow code from inside Xmlrpc-c, and also use an
       internal interface of xmlrpc_c::value.  This sliminess is one
       reason that this is Bryan's private code instead of part of the
       Xmlrpc-c package.
       
       Note that you must link with the dumpvalue.o object module from
       inside an Xmlrpc-c build tree.
    */

    dumpValue("", result.cValueP);
}



static list<string>
parseWordList(string const& wordString) {

    list<string> retval;

    unsigned int pos;

    pos = 0;

    while (pos < wordString.length()) {
        pos = wordString.find_first_not_of(' ', pos);

        if (pos < wordString.length()) {
            unsigned int const end = wordString.find_first_of(' ', pos);

            retval.push_back(wordString.substr(pos, end - pos));

            pos = end;
        }
    }
    return retval;
}
              


static void
parseCommand(string           const& cmd,
             string *         const  methodNameP,
             vector<string> * const  paramListP) {

    list<string> const wordList(parseWordList(cmd));

    list<string>::const_iterator cmdWordP;

    cmdWordP = wordList.begin();

    if (cmdWordP == wordList.end())
        throwf("Command '%s' does not have a method name", cmd.c_str());
    else {
        *methodNameP = *cmdWordP++;

        *paramListP = vector<string>();  // Start empty
        
        while (cmdWordP != wordList.end())
            paramListP->push_back(*cmdWordP++);
    }
}



static void
doCommand(client_xml *   const  clientP,
          string         const& methodName,
          vector<string> const& paramArgs) {

    value result;

    callWithClient(clientP, methodName, paramListFromParamArgs(paramArgs),
                   &result);

    try {
        dumpResult(result);
    } catch(exception const& e) {
        throwf("Error showing result after RPC completed normally.  %s",
               e.what());
    }
}



static void
getCommand(string * const cmdP,
           bool*    const eofP) {

    const char * cmd;

    cmd = readline(">");

    *eofP = (cmd == NULL);

    if (cmd != NULL) {
        *cmdP = string(cmd);

        free(const_cast<char *>(cmd));
    }
}



static void
doInteractive(client_xml * const clientP) {

    bool quitRequested;

    quitRequested = false;

    while (!quitRequested) {
        string cmd;
        bool eof;

        getCommand(&cmd, &eof);

        if (eof) {
            quitRequested = true;
            cout << endl;
        } else {
            try {
                string methodName;
                vector<string> paramArgs;

                parseCommand(cmd, &methodName, &paramArgs);

                doCommand(clientP, methodName, paramArgs);
            } catch (exception const& e) {
                cout << "Command failed.  " << e.what() << endl;
            }
        }
    }
}



int 
main(int           const argc, 
     const char ** const argv) {

    try {
        cmdlineInfo cmdline(argc, argv);

        signal(SIGPIPE, SIG_IGN);

        clientXmlTransport_pstream myTransport(
            clientXmlTransport_pstream::constrOpt()
            .fd(cmdline.serverfd));

        client_xml myClient(&myTransport);

        if (cmdline.interactive) {
            if (cmdline.serverfd == STDIN_FILENO ||
                cmdline.serverfd == STDOUT_FILENO)
                throwf("Can't use Stdin or Stdout for the server fd when "
                       "running interactively.");
            doInteractive(&myClient);
        } else
            doCommand(&myClient, cmdline.methodName, cmdline.params);

    } catch (exception const& e) {
        cerr << "Failed.  " << e.what() << endl;
    } catch (...) {
        cerr << "Code threw unrecognized exception" << endl;
        abort();
    }
    return 0;
}
