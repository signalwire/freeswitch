/*******************************************************************************

    KHOMP generic endpoint/channel library.
    Copyright (C) 2007-2010 Khomp Ind. & Com.

  The contents of this file are subject to the Mozilla Public License 
  Version 1.1 (the "License"); you may not use this file except in compliance 
  with the License. You may obtain a copy of the License at 
  http://www.mozilla.org/MPL/ 

  Software distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
  the specific language governing rights and limitations under the License.

  Alternatively, the contents of this file may be used under the terms of the
  "GNU Lesser General Public License 2.1" license (the “LGPL" License), in which
  case the provisions of "LGPL License" are applicable instead of those above.

  If you wish to allow use of your version of this file only under the terms of
  the LGPL License and not to allow others to use your version of this file 
  under the MPL, indicate your decision by deleting the provisions above and 
  replace them with the notice and other provisions required by the LGPL 
  License. If you do not delete the provisions above, a recipient may use your 
  version of this file under either the MPL or the LGPL License.

  The LGPL header follows below:

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation, 
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*******************************************************************************/

#ifndef _CLI_H_
#define _CLI_H_

#include "globals.h"
#include "logger.h"

struct Command 
{
    /* print in client the usage */
    void printUsage(switch_stream_handle_t *stream)
    { 
        if(stream)
        {
            printBrief(stream);
            K::Logger::Logg2(C_CLI,stream, 
"------------------------------ Description --------------------------------"); 
            K::Logger::Logg2(C_CLI,stream, 
"---------------------------------------------------------------------------"); 
            K::Logger::Logg2(C_CLI,stream,(char*) usage.c_str()); 
            K::Logger::Logg2(C_CLI,stream, 
"---------------------------------------------------------------------------"); 
        }
        else
        {
            LOG(ERROR,FMT("Invalid stream for commmand: %s") % complete_name);
        }
    }

    /* print in client the brief */
    void printBrief(switch_stream_handle_t *stream)
    {
        if(stream)
        {
            K::Logger::Logg2(C_CLI,stream, 
"---------------------------------------------------------------------------"); 
            K::Logger::Logg2(C_CLI,stream, 
"-------------------------------- Brief ------------------------------------"); 
            K::Logger::Logg2(C_CLI,stream, 
"---------------------------------------------------------------------------"); 
            K::Logger::Logg2(C_CLI,stream,(char*) brief.c_str()); 
            K::Logger::Logg2(C_CLI,stream, 
"---------------------------------------------------------------------------"); 
        }
        else
        {
            LOG(ERROR,FMT("Invalid stream for commmand: %s") % complete_name);
        }
    }

    /* pure virtual */
    virtual bool execute(int argc, char *argv[]) = 0;

    std::string complete_name;         /* specify the command in console */
    std::vector<std::string> options;  /* extra options for command */
    std::string brief;                 /* brief of the command, a path */
    std::string usage;                 /* usage of the command, a help */
};

struct CommandXMLOutput : public Command
{
    void createRoot(const char *name)
    {
        root = switch_xml_new(name);
    }

    void insertXML(switch_xml_t xml)
    {
        switch_xml_insert(xml,root,0);
    }

    void clearRoot()
    {
        if(root) 
        {
            switch_xml_free(root);
            root = NULL;
        }
    }

    void printXMLOutput(switch_stream_handle_t *stream)
    {
        K::Logger::Logg2(C_CLI,stream,switch_xml_toxml(root,SWITCH_FALSE)); 
    }

    CommandXMLOutput() : root(NULL) {};

    switch_xml_t root;                 /* for commands that ouput as xml */
};

struct Cli 
{
    /* Useful definitions --------------------------------------------------- */
    typedef switch_status_t (APIFunc)(const char*, switch_core_session_t*, switch_stream_handle_t*);
    typedef std::vector<Command*> Commands;

    /* Define the output types form commands */
    typedef enum 
    {
        VERBOSE = 1,
        CONCISE,
        DETAILED,
        XML
    } OutputType;

    /* register our commands, but you must create the command function */
    static void registerCommands(APIFunc func,switch_loadable_module_interface_t **mod_int);
    
    /* delete the commands */
    static void unregisterCommands()
    {
        switch_console_set_complete("del khomp");
    }

    /* stream is very useful */
    static void setStream(switch_stream_handle_t *s)
    {
        if(!s)
        {
            LOG(ERROR,"Invalid stream passed");
            return;
        }

        stream = s;
    }

    /* gets the khomp usage */
    static void printKhompUsage()
    {
        if(stream)
        {
            K::Logger::Logg2(C_CLI,stream,(char*) _khomp_usage.c_str());
        }
        else
        {
            LOG(ERROR,"Invalid stream for command: printKhompUsage");
        }
    }

    /* is responsible for parse and execute all commands */
    static bool parseCommands(int argc, char *argv[]);

    /* The Commands --------------------------------------------------------- */

    /* khomp summary */
    static struct _KhompSummary : public CommandXMLOutput
    {
        _KhompSummary(bool on_cli_term = true):
        CommandXMLOutput(),
        _on_cli_term(on_cli_term),
        xdevs(NULL)
        {
            complete_name = "summary";

            options.push_back("verbose");
            options.push_back("concise");
            options.push_back("xml");

            brief = "Print system info.";                                            
            usage =                                                            \
"Prints detailed info about the system like API version and \n"                \
"boards characteristics like DSPs version.\n\n"                                \
"Usage: khomp summary [concise|verbose|xml]";

            _commands.push_back(this);
        };

        bool execute(int argc, char *argv[]);
        bool _on_cli_term;     /* indicates if message is sent to fs_cli */
        switch_xml_t xdevs;  /* support xml needed to help the interation */
    } KhompSummary;

    /* khomp show calls */
    static struct _KhompShowCalls : public Command
    {
        _KhompShowCalls()
        {
            complete_name = "show calls";
            brief  =                                                           \
"Show each Khomp channel which have more than one call state associated.";

            usage =                                                            \
"Show each Khomp channel which have more than one call state associated.\n\n"  \
"Usage: khomp show calls [<board> [<channel>]]";

            _commands.push_back(this);
        };

        bool execute(int argc, char *argv[]);

        /* support function for _KhompShowCalls */
        void showCalls(unsigned int device, unsigned int object, std::string &buffer);
    } KhompShowCalls;

    /* khomp channels disconnect */
    static struct _KhompChannelsDisconnect : public Command
    {
        _KhompChannelsDisconnect()
        {
            complete_name = "channels disconnect";
            brief = "Disconnect a(ll) channel(s).";                                                              
            usage =                                                            \
"Disconnects channels in boards, or specific board/channel if parameter \n"    \
"is supplied.\n\n"                                                             \
"Usage: khomp channels disconnect {all | <board> all | <board> <channel>}\n"   \
"\tboard   -- Number of the board (start from 0).\n"                           \
"\tchannel -- Number of the channel (start from 0).\n"                         \
"e.g. khomp channels disconnect all - Disconnect all channels of all boards.\n"\
"e.g. khomp channels disconnect 0 5 - Disconnect channel 5 of board 0.";              
            _commands.push_back(this);
        };

        bool execute(int argc, char *argv[]);

        /* support function for _KhompChannelsDisconnect */
        bool forceDisconnect(unsigned int device, unsigned int channel);
    } KhompChannelsDisconnect;

    /* khomp channels unblock */
    static struct _KhompChannelsUnblock : public Command
    {   
        _KhompChannelsUnblock()
        {
            complete_name = "channels unblock";
            brief = "Unblock a(ll) channel(s).";

            usage =                                                            \
"The board will request to the PBX or network where it is connected to \n"     \
"unblock the channel if its blocked.\n\n"                                      \
"Usage: khomp channels unblock {all | <board> all | <board> <channel>}\n"      \
"\tboard   -- Number of the board (start from 0).\n"                           \
"\tchannel -- Number of the channel (start from 0).\n"                         \
"e.g. khomp channels unblock all   - Unblock all channels of all boards.\n"    \
"e.g. khomp channels unblock 0 all - Unblock all channels of board 0.\n"       \
"e.g. khomp channels unblock 1 20  - Unblock channel 20 of board 1.";

            _commands.push_back(this);
        };

        bool execute(int argc, char *argv[]);
    } KhompChannelsUnblock;

    /* khomp show statistics */
    static struct _KhompShowStatistics : public CommandXMLOutput
    {
        _KhompShowStatistics() : CommandXMLOutput(), xdevs(NULL)
        {
            complete_name = "show statistics";

            options.push_back("verbose");
            options.push_back("detailed");
            options.push_back("xml");

            brief = "Shows statistics of the channels.";

            usage =                                                            \
"Shows statistics of the channels, like number of calls incoming \n"           \
"and outgoing, status, status time.\n\n"                                       \
"Usage: khomp show statistics [{{verbose|xml} [<board> [<channel>]]} | \n"     \
"                              {detailed <board> <channel>}]\n"                \
"\tboard   -- Number of the board (start from 0).\n"                           \
"\tchannel -- Number of the channel (start from 0).\n"                         \
"e.g. khomp channels statistics              - Shows general statistics \n"    \
"                                              of all boards.\n"               \
"e.g. khomp channels statistics 0            - Shows general statistics \n"    \
"                                              of board 0.\n"                  \
"e.g. khomp channels statistics verbose      - Shows general statistics \n"    \
"                                              of all boards.\n"               \
"e.g. khomp channels statistics verbose  0   - Shows general statistics \n"    \
"                                              of board 0.\n"                  \
"e.g. khomp channels statistics detailed 0 2 - Shows detailed statistics \n"   \
"                                              of channel 2 on board 0.";

            _commands.push_back(this);
        };

        bool execute(int argc, char *argv[]);

        /* support functions */
        void cliStatistics(unsigned int device, OutputType output_type);
        void cliDetailedStatistics(unsigned int device, unsigned int channel, OutputType output_type);
        switch_xml_t xdevs;  /* support xml needed to help the interation */

    } KhompShowStatistics;

    /* khomp show channels */
    static struct _KhompShowChannels: public CommandXMLOutput
    {
        _KhompShowChannels() : CommandXMLOutput(), xdev(NULL)
        {
            complete_name = "show channels";

            options.push_back("verbose");
            options.push_back("concise");
            options.push_back("xml");

            brief = "Show all channels status.";                                                                               
            usage =                                                            \
"List the status of each channel, both on asterisk point of view and on \n"    \
"khomp API point of view.\n\n"                                                 \
"Usage: \n"                                                                    \
"khomp show channels [{<board> [<channel>]} | \n"                              \
                          "{{concise|verbose|xml} [<board> [<channel>]]}]\n"   \
"\tboard -- Number of the board (start from 0).\n"                             \
"e.g. khomp show channels - List status of all channels of all boards.\n"      \
"e.g. khomp show channels concise 0 - List status of all channels of \n"       \
"                                     board 0 in a concise way.\n"             \
"e.g. khomp show channels xml 0     - List status of all channels of \n"       \
"                                     board 0 in a xml structure.";

            _commands.push_back(this);
        };

        /* support function for _KhompShowChannels */
        void showChannel(unsigned int device, unsigned int channel, OutputType output_type = Cli::VERBOSE);
        void showChannels(unsigned int device, OutputType output_type = Cli::VERBOSE);

        bool execute(int argc, char *argv[]);
        switch_xml_t xdev; /* support xml needed to help the interation */

    } KhompShowChannels;
    
    /* khomp show links */
    static struct _KhompShowLinks: public CommandXMLOutput
    {
        _KhompShowLinks() : CommandXMLOutput(), xdev(NULL)
        {
            complete_name = "show links";

            options.push_back("verbose");
            options.push_back("concise");
            options.push_back("xml");
            options.push_back("errors");
            options.push_back("errors verbose");
            options.push_back("errors concise");
            options.push_back("errors xml");

            brief = "Show E1 link(s) status/errors counters in a concise \n"   \
            "way or not.";

            usage =                                                            \
"Prints information about the signaling, syncronization and general \n"        \
"status/the error counters of each link on the board. It prints in \n"         \
"a concise way for parsing facilities.\n\n"                                    \
"Usage: \n"                                                                    \
"khomp show links [[errors] [{<board>} | {{concise|verbose|xml} [<board>]}]]\n"\
"e.g. khomp show links          - Show all links of all boards.\n"             \
"e.g. khomp show links xml      - Show all links of all boards in xml.\n"      \
"e.g. khomp show links errors   - Show error counters of all links of \n"      \
"                                 all boards.\n"                               \
"e.g. khomp show links errors 0 - Show error counters of all links of \n"      \
"                                 board 0.";

            _commands.push_back(this);
        };

        /* support function for _KhompShowLinks */
        void showLinks(unsigned int device, OutputType output_type = Cli::VERBOSE);
        void showErrors(unsigned int device, OutputType output_type = Cli::VERBOSE);
        std::string getLinkStatus(int dev, int obj, Verbose::Presentation fmt);

        bool execute(int argc, char *argv[]);
        switch_xml_t xdev; /* support xml needed to help the interation */
    } KhompShowLinks;

    /* khomp clear links */
    static struct _KhompClearLinks: public Command
    {
        _KhompClearLinks()
        {
            complete_name = "clear links";
    
            brief = "Clear the error counters of the links.";

            usage =                                                            \
"Clear the error counters of the links.\n\n"                                   \
"Usage: khomp clear links [<board> [<link>]]\n"                                \
"\tboard -- Number of the board (start from 0).\n"                             \
"\tlink  -- Number of the link (start from 0).\n"                              \
"e.g. khomp clear links 0 -- Clear error counters of all links of board 0.";

            _commands.push_back(this);
        };

        /* support function for _KhompClearLinks */
        void clearLink(unsigned int device, unsigned int link);

        bool execute(int argc, char *argv[]);
    } KhompClearLinks;

    /* khomp clear statistics */
    static struct _KhompClearStatistics: public Command
    {
        _KhompClearStatistics()
        {
            complete_name = "clear statistics";
    
            brief = "Clear statistics of the channels.";

            usage =                                                            \
"Clear statistics of the channels, like number of calls incoming \n"           \
"and outgoing, status, status time.\n\n"                                       \
"Usage: khomp clear statistics [<board> [<channel>]]\n"                        \
"\tboard   -- Number of the board (start from 0).\n"                           \
"\tchannel -- Number of the channel (start from 0).\n"                         \
"e.g. khomp clear statistics 0 -- Clear statistics of board 0."; 
            _commands.push_back(this);
        };

        bool execute(int argc, char *argv[]);
    } KhompClearStatistics;


    /* khomp reset links */
    static struct _KhompResetLinks: public Command
    {
        _KhompResetLinks()
        {
            complete_name = "reset links";
    
            brief = "Reset the specified link.";

            usage =                                                            \
"Reset the specified link.\n\n"                                                \
"Usage: khomp reset links [<board> [<link>]]\n"                                \
"\tboard -- Number of the board (start from 0).\n"                             \
"\tlink  -- Number of the link (start from 0).\n"                              \
"e.g. khomp reset links 0 1 -- Reset link 1 of board 0.";

            _commands.push_back(this);
        };

        /* support function for _KhompResetLinks */
        void resetLink(unsigned int device, unsigned int link);

        bool execute(int argc, char *argv[]);
    } KhompResetLinks;
   
    /* khomp sms */
    static struct _KhompSMS : public Command
    {
        _KhompSMS()
        {
            complete_name = "sms";
            brief = "Send an SMS message using a Khomp KGSM board.";

            usage =                                                            \
"Send an SMS message using a Khomp KGSM board.\n\n"                            \
"Usage: khomp sms <device> <destination> <message..>\n"                        \
"\tdevice      -- Device to use (same string used in Dial for \n"              \
"\t               channel allocation).\n"                                      \
"\tdestination -- Phone number of the destination.\n"                          \
"\tmessage     -- Message to send.\n"                                          \
"e.g. khomp sms b0 99887766 Oi, tudo bem?";

            _commands.push_back(this);
        };

        bool execute(int argc, char *argv[]);
    } KhompSMS;

    /* khomp log console */
    static struct _KhompLogConsole : public Command
    {
        _KhompLogConsole()
        {
            complete_name = "log console";

            options.push_back("errors");
            options.push_back("warnings");
            options.push_back("messages");
            options.push_back("events");
            options.push_back("commands");
            options.push_back("audio");
            options.push_back("modem");
            options.push_back("link");
            options.push_back("cas");
            options.push_back("standard");
            options.push_back("all");
            options.push_back("no");
            options.push_back("just");

            brief = "Enables/disables showing console messages for the channel.";
            usage =                                                            \
"Enables/disables showing channel messages, where <options> can be:\n"         \
"\terrors    -- Error messages, when something goes really \n"                 \
"\t             wrong. Enabled by default.\n"                                  \
"\twarnings  -- Warnings, used when something might not be \n"                 \
"\t             going as expected. Enabled by default.\n"                      \
"\tmessages  -- Generic messages, used to indicate some \n"                    \
"\t             information. Enabled by default.\n"                            \
"\tevents    -- Show received K3L events as console \n"                        \
"\t             messages. Disabled by default.\n"                              \
"\tcommands  -- Show sent K3L commands as console \n"                          \
"\t             messages. Disabled by default.\n"                              \
"\taudio     -- Enable messages for K3L audio events. \n"                      \
"\t             Disabled by default (very verbose!).\n"                        \
"\tmodem     -- Enable messages for data received from \n"                     \
"\t             KGSM modems. Disabled by default.\n"                           \
"\tlink      -- Enable logging of link status changes. \n"                     \
"\t             Enabled by default.\n"                                         \
"\tcas       -- Enable logging of MFCs and line state \n"                      \
"\t             changes in KPR board. Disabled by default.\n"                  \
"\tstandard  -- Special identifier, enable default \n"                         \
"\t             console messages.\n"                                           \
"\tall       -- Special identifier, enable ALL console \n"                     \
"\t             messages (should not be used naively).\n\n"                    \
"Usage: khomp log console <options>\n"                                         \
"e.g. khomp log console standard";

            _commands.push_back(this);
        }

        bool execute(int argc, char *argv[]);
    } KhompLogConsole;

    /* khomp log disk */
    static struct _KhompLogDisk : public Command
    {
        _KhompLogDisk()
        {
            complete_name = "log disk";

            options.push_back("errors");
            options.push_back("warnings");
            options.push_back("messages");
            options.push_back("events");
            options.push_back("commands");
            options.push_back("audio");
            options.push_back("modem");
            options.push_back("link");
            options.push_back("cas");
            options.push_back("functions");
            options.push_back("threads");
            options.push_back("locks");
            options.push_back("streams");
            options.push_back("standard");
            options.push_back("debugging");
            options.push_back("all");
            options.push_back("no");
            options.push_back("just");

            brief = "Enables/disables logging to file messages for the channel.";
            usage =                                                            \
"Enables/disables the logging of channel messages to disk, where <options> \n" \
"can be:\n"                                                                    \
"\terrors    -- Error messages, when something goes really \n"                 \
"\t             wrong. Enabled by default.\n"                                  \
"\twarnings  -- Warnings, used when something might not be \n"                 \
"\t             going as expected. Enabled by default.\n"                      \
"\tmessages  -- Generic messages, used to indicate some \n"                    \
"\t             information. Enabled by default.\n"                            \
"\tevents    -- Record received K3L events as log messages. \n"                \
"\t             Disabled by default.\n"                                        \
"\tcommands  -- Record sent K3L commands as log messages. \n"                  \
"\t             Disabled by default.\n"                                        \
"\taudio     -- Enable messages for K3L audio events. \n"                      \
"\t             Disabled by default (very verbose!).\n"                        \
"\tmodem     -- Enable messages for data received from \n"                     \
"\t             KGSM modems. Disabled by default.\n"                           \
"\tlink      -- Enable logging of link status changes. \n"                     \
"\t             Enabled by default.\n"                                         \
"\tcas       -- Enable logging of MFCs and line state \n"                      \
"\t             changes in KPR board. Disabled by default.\n"                  \
"\tfunctions -- Enable debugging for functions. Disabled \n"                   \
"\t             by default (should not be used naively!).\n"                   \
"\tthreads   -- Enable debugging for threads. Disabled by \n"                  \
"\t             default (should not be used naively!).\n"                      \
"\tlocks     -- Enable debugging for locks. Disabled by \n"                    \
"\t             default (should not be used naively!).\n"                      \
"\tstreams   -- Enable debugging for streams. Disabled by \n"                  \
"\t             default (should not be used naively!).\n"                      \
"\tstandard  -- Special identifier, enable default messages.\n"                \
"\tdebugging -- Special identifier, enable debugging messages \n"              \
"\t             (should not be used naively).\n"                               \
"\tall       -- Special identifier, enable ALL disk \n"                        \
"\t             messages (DO NOT USE THIS!).\n\n"                              \
"Usage: khomp log disk <options>\n"                                            \
"e.g. khomp log disk <options>";

            _commands.push_back(this);
        }

        bool execute(int argc, char *argv[]);
    } KhompLogDisk;

    /* khomp log trace k3l */
    static struct _KhompLogTraceK3L : public Command
    {
        _KhompLogTraceK3L()
        {
            complete_name = "log trace k3l";

            options.push_back("on");
            options.push_back("off");

            brief = "Set K3L tracing (debug) option.";

            usage =                                                            \
"Sets the low-level log for K3L API. Should not be set for long time \n"       \
"periods.\n\n"                                                                 \
"Usage: khomp log trace k3l {on|off}\n"                                        \
"e.g. khomp log trace k3l on";

            _commands.push_back(this);
        }

        bool execute(int argc, char *argv[]);
    } KhompLogTraceK3L;

    /* khomp log trace ISDN */
    static struct _KhompLogTraceISDN : public Command
    {
        _KhompLogTraceISDN()
        {
            complete_name = "log trace isdn";

            options.push_back("q931");
            options.push_back("lapd");
            options.push_back("system");
            options.push_back("off");

            brief = "Set ISDN signaling trace.";

            usage =                                                            \
"Sets the low-level log for ISDN signalling. Should not be set for \n"         \
"long time periods.\n\n"                                                       \
"Usage: khomp log trace isdn <what>[,<what2>[,..]]\n"                          \
"\twhat -- \"q931\", \"lapd\", \"system\" or \"off\" \n"                       \
"\t        (comma-separated values).\n"                                        \
"e.g. khomp log trace isdn q931,system";

            _commands.push_back(this);
        }

        bool execute(int argc, char *argv[]);
    } KhompLogTraceISDN;

    /* khomp log trace R2 */
    static struct _KhompLogTraceR2 : public Command
    {
        _KhompLogTraceR2()
        {
            complete_name = "log trace r2";

            options.push_back("on");
            options.push_back("off");

            brief = "Set R2 signaling trace.";

            usage =                                                            \
"Sets the low-level log monitor for R2 digital signalling. Should not \n"      \
"be set for long time periods.\n\n"                                            \
"Usage: khomp log trace r2 {on|off}\n"                                         \
"e.g. khomp log trace r2 on";

            _commands.push_back(this);
        }

        bool execute(int argc, char *argv[]);
    } KhompLogTraceR2;

    /* khomp get */
    static struct _KhompGet : public Command
    {
        _KhompGet()
        {
            complete_name = "get";

            options.push_back("dialplan");
            options.push_back("echo-canceller");
            options.push_back("auto-gain-control");
            options.push_back("out-of-band-dtmfs");
            options.push_back("suppression-delay");
            options.push_back("auto-fax-adjustment");
            options.push_back("fax-adjustment-timeout");
            options.push_back("pulse-forwarding");
            options.push_back("r2-strict-behaviour");
            options.push_back("r2-preconnect-wait");
            options.push_back("context-digital");
            options.push_back("context-fxs");
            options.push_back("context-fxo");
            options.push_back("context-gsm-call");
            options.push_back("context-gsm-sms");
            options.push_back("context-pr");
            options.push_back("log-to-console");
            options.push_back("log-to-disk");
            options.push_back("trace");
            options.push_back("output-volume");
            options.push_back("input-volume");
            options.push_back("fxs-global-orig");
            options.push_back("fxs-co-dialtone");
            options.push_back("fxs-bina");
            options.push_back("fxs-sharp-dial");
            options.push_back("disconnect-delay");
            options.push_back("delay-ringback-co");
            options.push_back("delay-ringback-pbx");
            options.push_back("ignore-letter-dtmfs");
            options.push_back("fxo-send-pre-audio");
            options.push_back("fxo-busy-disconnection");
            options.push_back("fxs-digit-timeout");
            options.push_back("drop-collect-call");
            options.push_back("kommuter-activation");
            options.push_back("kommuter-timeout");
            options.push_back("user-transfer-digits");
            options.push_back("flash-to-digits");
            options.push_back("accountcode");
            options.push_back("audio-packet-length");

            brief = "Get configuration options in the Khomp channel.";

            usage =                                                            \
"Usage: khomp get <option>\n"                                                  \
"<option>  -- Shown below, with a short description each.\n\n"                 \
"Option               Description\n"                                           \
"dialplan             Gets the Name of the dialplan module in use.\n"          \
"echo-canceller       Gets the echo cancellation procedures if they are \n"    \
"                      avaliable.\n"                                           \
"auto-gain-control    Gets the AGC procedures if they are avaliable.\n"        \
"out-of-band-dtmfs    Gets DTMFs to be sent out-of-band (using ast_frames) \n" \
"                      instead of in-band (audio).\n"                          \
"suppression-delay    Enable/disable the internal buffer for DTMF \n"          \
"                      suppression.\n"                                         \
"auto-fax-adjustment  Gets the automatic adjustment for FAX (mainly, \n"       \
"                      disables the echo canceller).\n"                        \
//"fax-adjustment-timeout"
"pulse-forwarding     Gets the forwarding of detected pulses as DTMF tones.\n" \
"r2-strict-behaviour  Gets the R2 protocol behavior while answering lines.\n"  \
"r2-preconnect-wait   Gets the R2 protocol delay to pre-connect after \n"      \
"                      sending ringback signal.\n"                             \
"context-digital      Context (may be a template) for receiving digital \n"    \
"                      (E1) calls.\n"                                          \
"context-fxs          Context (may be a template) for receiving FXS calls.\n"  \
"context-fxo          Context (may be a template) for receiving FXO calls.\n"  \
"context-gsm-call     Context (may be a template) for receiving GSM calls.\n"  \
"context-gsm-sms      Context (may be a template) for receiving GSM \n"        \
"                      messages.\n"                                            \
"context-pr           Context (may be a template) for receiving calls on \n"   \
"                      Passive Record boards.\n"                               \
"log-to-console       Gets the logging of messages to console.\n"              \
"log-to-disk          Gets the logging of messages to disk.\n"                 \
"trace                Gets the low level tracing.\n"                           \
"output-volume        Gets the volume multiplier to be applied by the \n"      \
"                      board over the output audio.\n"                         \
"input-volume         Gets the volume multiplier to be applied by the \n"      \
"                      board over the input audio.\n\n"                        \
"FOR ALL COMMANDS, CHECK THE DOCUMENTATION.";

            _commands.push_back(this);
        };

        bool execute(int argc, char *argv[]);
    } KhompGet;

    /* khomp set */
    static struct _KhompSet : public Command
    {
        _KhompSet()
        {
            complete_name = "set";

            options.push_back("dialplan");
            options.push_back("echo-canceller");
            options.push_back("auto-gain-control");
            options.push_back("out-of-band-dtmfs");
            options.push_back("suppression-delay");
            options.push_back("auto-fax-adjustment");
            options.push_back("fax-adjustment-timeout");
            options.push_back("pulse-forwarding");
            options.push_back("r2-strict-behaviour");
            options.push_back("r2-preconnect-wait");
            options.push_back("context-digital");
            options.push_back("context-fxs");
            options.push_back("context-fxo");
            options.push_back("context-gsm-call");
            options.push_back("context-gsm-sms");
            options.push_back("context-pr");
            options.push_back("log-to-console");
            options.push_back("log-to-disk");
            options.push_back("trace");
            options.push_back("output-volume");
            options.push_back("input-volume");
            options.push_back("fxs-global-orig");
            options.push_back("fxs-co-dialtone");
            options.push_back("fxs-bina");
            options.push_back("fxs-sharp-dial");
            options.push_back("disconnect-delay");
            options.push_back("delay-ringback-co");
            options.push_back("delay-ringback-pbx");
            options.push_back("ignore-letter-dtmfs");
            options.push_back("fxo-send-pre-audio");
            options.push_back("fxo-busy-disconnection");
            options.push_back("fxs-digit-timeout");
            options.push_back("drop-collect-call");
            options.push_back("kommuter-activation");
            options.push_back("kommuter-timeout");
            options.push_back("user-transfer-digits");
            options.push_back("flash-to-digits");
            options.push_back("accountcode");
            options.push_back("audio-packet-length");

            brief = "Ajust configuration options in the Khomp channel.";

            usage =                                                            \
"Usage: khomp set <option> <value>\n"                                          \
"\t<option>  -- Shown below, with a short description each.\n"                 \
"\t<value>   -- Depends on the option. Check the documentation for \n"         \
"\t             more info.\n\n"                                                \
"Option               Description\n"                                           \
"dialplan             Sets the Name of the dialplan module in use.\n"          \
"echo-canceller       Sets the echo cancellation procedures if they are \n"    \
"                      avaliable.\n"                                           \
"auto-gain-control    Sets the AGC procedures if they are avaliable.\n"        \
"out-of-band-dtmfs    Sets DTMFs to be sent out-of-band (using ast_frames) \n" \
"                      instead of in-band (audio).\n"                          \
"suppression-delay    Enable/disable the internal buffer for DTMF \n"          \
"                      suppression.\n"                                         \
"auto-fax-adjustment  Sets the automatic adjustment for FAX (mainly, \n"       \
"                      disables the echo canceller).\n"                        \
"pulse-forwarding     Sets the forwarding of detected pulses as DTMF tones.\n" \
"r2-strict-behaviour  Sets the R2 protocol behavior while answering lines.\n"  \
"r2-preconnect-wait   Sets the R2 protocol delay to pre-connect after \n"      \
"                      sending ringback signal.\n"                             \
"context-digital      Context (may be a template) for receiving digital \n"    \
"                      (E1) calls.\n"                                          \
"context-fxs          Context (may be a template) for receiving FXS calls.\n"  \
"context-fxo          Context (may be a template) for receiving FXO calls.\n"  \
"context-gsm-call     Context (may be a template) for receiving GSM calls.\n"  \
"context-gsm-sms      Context (may be a template) for receiving GSM \n"        \
"                      messages.\n"                                            \
"context-pr           Context (may be a template) for receiving calls on \n"   \
"                      Passive Record boards.\n"                               \
"log-to-console       Sets the logging of messages to console.\n"              \
"log-to-disk          Sets the logging of messages to disk.\n"                 \
"trace                Sets the low level tracing.\n"                           \
"output-volume        Sets the volume multiplier to be applied by the \n"      \
"                      board over the output audio.\n"                         \
"input-volume         Sets the volume multiplier to be applied by the \n"      \
"                      board over the input audio.\n\n"                        \
"FOR ALL COMMANDS, CHECK THE DOCUMENTATION.";

            _commands.push_back(this);
        };

        bool execute(int argc, char *argv[]);
    } KhompSet;

    /* khomp log rotate */
    static struct _KhompLogRotate : public Command 
    {
        _KhompLogRotate()
        {
            complete_name = "log rotate";
            brief = "Rotate the files where the messages are being logged.";

            usage =                                                            \
"Rotate the files where the messages are being logged.\n\n"                    \
"Usage: khomp log rotate";

            _commands.push_back(this);
        }

        bool execute(int argc, char *argv[]);
    } KhompLogRotate;

    /* khomp log status */
    static struct _KhompLogStatus : public Command
    {
        _KhompLogStatus()
        {
            complete_name = "log status";
            brief = "Show the status of the messages.";

            usage =                                                            \
"Show the status of the message system (enabled/disabled messages).\n\n"       \
"Usage: khomp log status";

            _commands.push_back(this);
        }

        bool execute(int argc, char *argv[]);
    } KhompLogStatus;

    /* khomp revision */
    static struct _KhompRevision : public Command
    {
        _KhompRevision()
        {
            complete_name = "revision";
            brief = "Show revision number.";

            usage =                                                            \
"Show the internal revision number for this channel.\n\n"                      \
"Usage: khomp revision";

            _commands.push_back(this);
        }

        bool execute(int argc, char *argv[]);
    } KhompRevision;

    /* khomp dump config */
    static struct _KhompDumpConfig : public Command
    {
        _KhompDumpConfig()
        {
            complete_name = "dump config";
            brief = "Dump configuration values on screen.";

            usage =                                                            \
"\nUsage: khomp dump config\n\n"                                               \
"Dump configuration values loaded on memory.\n ";                              
            
            _commands.push_back(this);
        }

        /* just to hide unavaible options */
        bool removeUnavaible(const std::string &s)
        {
            if(s == "atxfer" || s == "blindxfer" || s == "callgroup" ||
               s == "mohclass" || s == "native-bridge" || s == "recording" ||
               s == "record-prefix" || s == "transferdigittimeout" ||
               s == "pickupgroup" || s == "has-ctbus" || 
               s == "user-transfer-digits")
                return true;
            return false;
        }
        
        bool execute(int argc, char *argv[]);
    } KhompDumpConfig;

    /* khomp send command */
    static struct _KhompSendCommand : public Command
    {
        _KhompSendCommand()
        {
            complete_name = "send command";
            brief = "Send an K3L API command directly to the board.";

            usage =                                                            \
"Send an K3L API command directly to the board.\n"                             \
"WARNING: This command is for debugging only - just use if you know \n"        \
"what you are doing!\n\n"                                                      \
"Usage: khomp send command <board> <channel> <command> [argument]\n"           \
"\tboard    -- Number of the board (start from 0).\n"                          \
"\tchannel  -- Number of the channel (start from 0).\n"                        \
"\tcommand  -- Number of API command (should be consulted in separate docs).\n"\
"\targument -- Optimal string to use as parameter for the command.\n"          \
"e.g. khomp send command 0 1 4 123456 - Send DTMFs 123456 throught \n"         \
"                                       channel 1 on board 0.";

            _commands.push_back(this);
        };

        bool execute(int argc, char *argv[]);
    } KhompSendCommand;

    /* khomp send raw */
    static struct _KhompSendRawCommand : public Command 
    {
        _KhompSendRawCommand()
        {
            complete_name = "send raw command";
            brief = "Send an command directly to a DSP at the board.";

            usage =                                                            \
"Send an command directly to some DSP at the board.\n"                         \
"WARNING: This command is for debugging only - just use if you know \n"        \
"what you are doing!\n\n"                                                      \
"Usage: khomp send raw command <board> <dsp> <c0> <c1> [...]\n"                \
"\tboard       -- Number of the board (start from 0).\n"                       \
"\tchannel     -- Number of the channel (start from 0).\n"                     \
"\tdsp         -- Number of the DSP.\n"                                        \
"\tc0, c1, ... -- Hexadecimal numbers for the command.\n"                      \
"e.g. khomp send raw command 0 1 0x4a 0x00 0x00 - Disables the internal \n"    \
"                                                 audio delay on board 0.";

            _commands.push_back(this);
        };

        bool execute(int argc, char *argv[]);
    } KhompSendRawCommand;
    
    /* khomp select sim */
    static struct _KhompSelectSim : public Command
    {
        _KhompSelectSim()
        {
            complete_name = "select sim";

            brief = "Select SIM card, available to Khomp KGSM boards.";

            usage =                                                            \
"Select SIM card, available to Khomp KGSM boards.\n\n"                         \
"Usage: khomp select sim <board> <channel> <sim_card>\n"                       \
"\tboard    -- Number of the board (start from 0).\n"                          \
"\tchannel  -- Number of the channel (start from 0), identifies \n"            \
"\t            the modem inside the board.\n"                                  \
"\tsim_card -- Number for the SIM card to select.\n"                           \
"e.g. khomp select sim 0 0 2";

            _commands.push_back(this);
        };

        bool execute(int argc, char *argv[]);
    } KhompSelectSim;

    /* khomp kommuter */
    static struct _KhompKommuterOnOff : public Command
    {
        _KhompKommuterOnOff()
        {
            complete_name = "kommuter";

            options.push_back("on");
            options.push_back("off");

            brief = "Activate and deactives the Kommuter devices.";

            usage =                                                            \
"Manually activates or deactivates all kommuters connected to this machine.\n" \
"This command will only be sent if kommuter-activation variable in \n"         \
"khomp.conf is set to manual. All kommuter devices will be configure with \n"  \
"the timeout defined in kommuter-timeout.\n\n"                                 \
"Usage: khomp kommuter {on|off}\n"                                             \
"\ton     -- Turn ON kommuter devices on this machine.\n"                      \
"\toff    -- Turn OFF kommuter devices on this machine.";

            _commands.push_back(this);
        };

        bool execute(int argc, char *argv[]);
    } KhompKommuterOnOff;
    
    /* khomp kommuter count */
    static struct _KhompKommuterCount : public Command
    {
        _KhompKommuterCount()
        {
            complete_name = "kommuter count";

            brief = "Prints the number of Kommuter devices.";

            usage =                                                            \
"Prints the number of Kommuter devices connected to this machine.\n\n"         \
"Usage: khomp kommuter count";

            _commands.push_back(this);
        };

        bool execute(int argc, char *argv[]);
    } KhompKommuterCount;

protected:
    static switch_stream_handle_t* stream;
    static switch_loadable_module_interface_t **module_interface;
    static Commands _commands;
    static std::string _khomp_usage;
};

/******************************************************************************/
/******************************* Command Tree *********************************

This tree is a overview for all commands in our module,
when a command is added, deleted or modified, please, update our tree.

+ khomp 
|
+ ---- channels 
|    |
-    + ---- disconnect
|    |
-    + ---- unblock
|
+ ---- show
|    | 
-    + ---- calls
|    |
-    + ---- channels
|    |    | 
-    -    + ---- verbose
|    |    |
-    -    + ---- concise
|    |
-    + ---- statistics
|    |    |  
-    -    + ---- detailed
|    |    |  
-    -    + ---- verbose 
|    |   
-    + ---- links
|         |
-         + ---- errors
|         |
-         + ---- verbose
|         |
-         + ---- concise
|    
+ ---- reset
|    | 
-    + ---- links
|
+ ---- clear
|    | 
-    + ---- links
|         |
-         + ---- errors
|         |
-         + ---- statistics
|
+ ---- log
|    |
-    + ---- console
|    |    |
-    -    + ---- errors
|    |    |
-    -    + ---- warnings
|    |    |
-    -    + ---- messages
|    |    |
-    -    + ---- events
|    |    |
-    -    + ---- commands
|    |    |
-    -    + ---- audio
|    |    |
-    -    + ---- link
|    |    |
-    -    + ---- cas
|    |    |
-    -    + ---- standard
|    |    |
-    -    + ---- all
|    |
-    + ---- disk
|    |    |
-    -    + ---- errors
|    |    |
-    -    + ---- warnings
|    |    |
-    -    + ---- messages
|    |    |
-    -    + ---- events
|    |    |
-    -    + ---- commands
|    |    |
-    -    + ---- audio
|    |    |
-    -    + ---- modem
|    |    |
-    -    + ---- link
|    |    |
-    -    + ---- cas
|    |    |
-    -    + ---- functions
|    |    |
-    -    + ---- threads
|    |    |
-    -    + ---- locks
|    |    |
-    -    + ---- streams
|    |    |
-    -    + ---- standard
|    |    |
-    -    + ---- debugging
|    |    |
-    -    + ---- all
|    |     
-    + ---- rotate
|    |
-    + ---- status
|    |
-    + ---- trace
|         |
-         + ---- isdn 
|         |    |
-         -    + ---- q931
|         |    |
-         -    + ---- lapd
|         |    |
-         -    + ---- system
|         |    |
-         -    + ---- off
|         |
-         + ---- k3l 
|         |    |
-         -    + ---- on
|         |    |
-         -    + ---- off
|         |
-         + ---- r2 
|              |
-              + ---- on
|              |
-              + ---- off
|
+ ---- send
|    |
-    + ---- command
|    |
-    + ---- raw
|
+ ---- get
|
+ ---- set
|
+ ---- select
|    |
-    + ---- sim
|
+ ---- sms
|
+ ---- summary
|    |
-    + ---- concise
|    |
-    + ---- verbose
|
+ ---- revision
|
+ ---- kommuter
     |
     + ---- on
     |
     + ---- off

 ******************************************************************************/
/******************************************************************************/

#endif 

