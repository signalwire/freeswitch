/* Copyright (C) 2005 by Steven A. Bone, sbone@pobox.com. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission. 
**  
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE. */

/* COMPILATION NOTE:
   Note that the Platform SDK headers and
   link libraries for Windows XP SP2 or newer are required to compile
   xmlrpc-c for this module.  If you are not using this server, it is 
   safe to exclude the xmlrpc_server_w32httpsys.c file from the xmlrpc
   project and these dependencies will not be required.  You can get the 
   latest platform SDK at 
   http://www.microsoft.com/msdownload/platformsdk/sdkupdate/
   Be sure after installation to choose the program to "register the PSDK
   directories with Visual Studio" so the newer headers are found.
*/

#include <string.h>
#include <stdio.h>
#include <xmlrpc-c/base.h>
#include <xmlrpc-c/server.h>
#include <xmlrpc-c/server_w32httpsys.h>


/*  SECURITY NOTE: Using HTTP Authorization

The current implementation of HTTP Authorization in the win32httpsys
server only uses basic Authorization.  This means the userid and password
is sent in clear-text over the network (Technically, it is Base64 encoded,
but this is essentially clear text).  This method is not secure, as it
can be captured and decoded.  The use of HTTP Basic Authorization with SSL
is considered much more secure.  See the note below for configuring SSL
support.
*/


/*  
HOWTO: Configure SSL for the XMLRPC-C Server

To use SSL you need an SSL certificate.  For testing purposes,
it is possible to create a self-signed SSL certificate.  To do so,
you must download the IIS 6.0 Resource Kit tools.  The current
URL to get the download link is http://support.microsoft.com/kb/840671
We will be using the SelfSSL version 1.0 from this toolkit for
this example.  The other tool you will need is httpcfg.exe, which
can be compiled from the sources in the Windows XP SP2 (or newer) Platform SDK,
or downloaded as part of the Windows XP SP2 Support Tools at the following URL:
http://www.microsoft.com/downloads/details.aspx?FamilyID=49ae8576-9bb9-4126-9761-ba8011fabf38&displaylang=en
The last assumption is that this procedure is being done on the machine that is
hosting the XMLRPC-C server application.

1) Make sure that IIS is installed, and you are running at least Windows XP SP2
or Windows Server 2003.  WARNING: This process will replace any existing IIS SSL
certificates currently installed.

2) In a command prompt, navigate to the directory of the
IIS Support Tools where the selfssl program exists (usually 
C:\Program Files\IIS Resources\SelfSSL).  Assuming (as we are for this example)
that we are going to run on port 8443, use the following command line (see the
documentation for all the flags):

selfssl /T /V:365 /P:8443

3) In the Control Panel, Administrative tools, run the Internet Information Services
program.  Drill down to the Default Web Site.  Right-click it and choose Properties.
On the "Web Site" tab, you will notice that the SSL port is now set to 8443.  Change
it back to 443.  On the Directory Security tab, pick "View Certificate".  In the 
"Details" tab, select the "Thumbprint" line.  The edit box below the listbox will
display a series of hex numbers.  Copy these to the clipboard and paste into notepad.
OK yourself out of the IIS program.

4) Remove all the spaces in the hex string so you are left with a string with no spaces.
This is your SSL Thumbprint hash which you will need in the next step.

5) At your command prompt, navigate to the support tools directory (or the location
where you built httpcfg.exe) - usually C:\Program Files\Support Tools.  Run the following
command line, replacing both the brackets and text with your thumbprint hash from step 4 above:

httpcfg.exe set ssl -i 0.0.0.0:8443 -h <replace with thumbprint hash> -g "{2bb50d9c-7f6a-4d6f-873d-5aee7fb43290}" -c "MY" -t "" -n ""

6) You can check the setup by performing a "httpcfg.exe query ssl" if you wish.

7) Modify the example server code below to use SSL.  Set the xmlrpc_server_httpsys_parms.useSSL
to '1' and the xmlrpc_server_httpsys_parms.portNum to be '8443'.  You can test the server by using 
IE to browse to the URL https://127.0.0.1:8443/rpc2.  An error 405 (Resource not allowed) is the 
expected result if everything is working properly.

NOTE: Testing clients with a 'test' or not real SSL certificate involves changing some of the default
code in the client samples, as by default the transports will fail if there are any issues with the
certificate.  The WinInet transport as of 1.2 has a transport-specific setting to allow 
invalid SSL certificates.  See the libxmlrpc_client.html documentation for more details.

NOTE: Failure to follow all the steps listed above correctly will result in no application
errors, event log messages, or HTTP.SYS log messages indicating failure or the cause.  If
anyone can provide information on debugging SSL certificate issues in HTTP.SYS, please
submit to us!
*/


static xmlrpc_value *
sample_add(xmlrpc_env *   const env, 
           xmlrpc_value * const param_array, 
           void *         const user_data ) {

    xmlrpc_int32 x, y, z;

    /* Parse our argument array. */
    xmlrpc_decompose_value(env, param_array, "(ii)", &x, &y);
    if (env->fault_occurred)
        return NULL;

    /* Add our two numbers. */
    z = x + y;

    /* Return our result. */
    return xmlrpc_build_value(env, "i", z);
}

static void handleAuthorization(
		xmlrpc_env * envP,
        char * userid,
        char * password)
{
	if (strcmp(userid,"jrandom")==0 && strcmp(password,"secret")==0)
		return;

	xmlrpc_env_set_fault( envP, XMLRPC_REQUEST_REFUSED_ERROR, 
						  "Username and/or password do not match.");
}

int __cdecl wmain( int argc, wchar_t * argv[])
{
	xmlrpc_server_httpsys_parms serverparm;
    xmlrpc_registry * registryP;
    xmlrpc_env env;

	xmlrpc_env_init(&env);

	registryP = xmlrpc_registry_new(&env);

    xmlrpc_registry_add_method(
        &env, registryP, NULL, "sample.add", &sample_add, NULL);

    wprintf(L"Starting XML-RPC server...\n");

	//Sets the port number we are listening on
	serverparm.portNum=8080;

	//if this is set, we will use the authorization function
	//serverparm.authfn=NULL;
	serverparm.authfn=&handleAuthorization;

	//set the logging level and log file
	serverparm.logLevel=2;
	serverparm.logFile="C:\\httpsysserverlog.txt";

	//set the use of SSL
	serverparm.useSSL=0;

    serverparm.registryP = registryP;

    xmlrpc_server_httpsys(&env, &serverparm, XMLRPC_HSSIZE(authfn));

	wprintf(L"Stopping XML-RPC server...\n");

	xmlrpc_registry_free(registryP);
	xmlrpc_env_clean(&env);

    return 0;
}
