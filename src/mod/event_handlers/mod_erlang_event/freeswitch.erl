%% The contents of this file are subject to the Mozilla Public License
%% Version 1.1 (the "License"); you may not use this file except in
%% compliance with the License. You may obtain a copy of the License at
%% http://www.mozilla.org/MPL/
%% 
%% Software distributed under the License is distributed on an "AS IS"
%% basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
%% License for the specific language governing rights and limitations
%% under the License.
%% 
%% @author Andrew Thompson <andrew AT hijacked DOT us>
%% @copyright 2008-2009 Andrew Thompson
%% @doc A module for interfacing with FreeSWITCH using mod_erlang_event.

-module(freeswitch).

-export([send/2, api/3, api/2, bgapi/3, bgapi/4, event/2, session_event/2,
		nixevent/2, session_nixevent/2, noevents/1, session_noevents/1, close/1,
		get_event_header/2, get_event_body/1,
		get_event_name/1, getpid/1, sendmsg/3,
		sendevent/3, sendevent_custom/3, handlecall/2, handlecall/3, start_fetch_handler/5,
		start_log_handler/4, start_event_handler/4, fetch_reply/3]).
-define(TIMEOUT, 5000).

%% @doc Return the value for a specific header in an event or `{error,notfound}'.
get_event_header([], _Needle) ->
	{error, notfound};
get_event_header({event, Headers}, Needle) when is_list(Headers) ->
	get_event_header(Headers, Needle);
get_event_header([undefined | Headers], Needle) ->
	get_event_header(Headers, Needle);
get_event_header([UUID | Headers], Needle) when is_list(UUID) ->
	get_event_header(Headers, Needle);
get_event_header([{Key,Value} | Headers], Needle) ->
	case Key of
		Needle ->
			Value;
		_ ->
			get_event_header(Headers, Needle)
	end.

%% @doc Return the name of the event.
get_event_name(Event) ->
	get_event_header(Event, "Event-Name").

%% @doc Return the body of the event or `{error, notfound}' if no event body.
get_event_body(Event) ->
	get_event_header(Event, "body").

%% @doc Send a raw term to FreeSWITCH. Returns the reply or `timeout' on a
%% timeout.
send(Node, Term) ->
	{send, Node} ! Term,
	receive
		Response ->
			Response
	after ?TIMEOUT ->
			timeout
	end.

fetch_reply(Node, FetchID, Reply) ->
	{send, Node} ! {fetch_reply, FetchID, Reply},
	receive
		{ok, FetchID} ->
			ok;
		{error, FetchID, Reason} ->
			{error, Reason}
	after ?TIMEOUT ->
			timeout
	end.


%% @doc Make a blocking API call to FreeSWITCH. The result of the API call is
%% returned or `timeout' if FreeSWITCH fails to respond.
api(Node, Cmd, Args) ->
	{api, Node} ! {api, Cmd, Args},
	receive
		{ok, X} -> 
			{ok, X};
		{error, X} ->
			{error, X}
	after ?TIMEOUT ->
		timeout
	end.

%% @doc Same as @link{api/3} except there's no additional arguments.
api(Node, Cmd) ->
	api(Node, Cmd, "").

%% @doc Make a backgrounded API call to FreeSWITCH. The asynchronous reply is
%% sent to calling process after it is received. This function
%% returns the result of the initial bgapi call or `timeout' if FreeSWITCH fails
%% to respond.
-spec(bgapi/3 :: (Node :: atom(), Cmd :: atom(), Args :: string()) -> {'ok', string()} | {'error', any()} | 'timeout').
bgapi(Node, Cmd, Args) ->
	Self = self(),
	% spawn a new process so that both responses go here instead of directly to
	% the calling process.
	spawn(fun() ->
		{bgapi, Node} ! {bgapi, Cmd, Args},
		receive
			{error, Reason} ->
				% send the error condition to the calling process
				Self ! {api, {error, Reason}};
			{ok, JobID} ->
				% send the reply to the calling process
				Self ! {api, {ok, JobID}},
				receive % wait for the job's reply
					{bgok, JobID, Reply} ->
						% send the actual command output back to the calling process
						Self ! {bgok, JobID, Reply};
					{bgerror, JobID, Reply} ->
						Self ! {bgerror, JobID, Reply}
				end
		after ?TIMEOUT ->
			% send a timeout to the calling process
			Self ! {api, timeout}
		end
	end),

	% get the initial result of the command, NOT the asynchronous response, and
	% return it
	receive
		{api, X} -> X
	end.

%% @doc Make a backgrounded API call to FreeSWITCH. The asynchronous reply is
%% passed as the argument to `Fun' after it is received. This function
%% returns the result of the initial bgapi call or `timeout' if FreeSWITCH fails
%% to respond.
-spec(bgapi/4 :: (Node :: atom(), Cmd :: atom(), Args :: string(), Fun :: fun()) -> 'ok' | {'error', any()} | 'timeout').
bgapi(Node, Cmd, Args, Fun) ->
	Self = self(),
	% spawn a new process so that both responses go here instead of directly to
	% the calling process.
	spawn(fun() ->
		{bgapi, Node} ! {bgapi, Cmd, Args},
		receive
			{error, Reason} ->
				% send the error condition to the calling process
				Self ! {api, {error, Reason}};
			{ok, JobID} ->
				% send the reply to the calling process
				Self ! {api, ok},
				receive % wait for the job's reply
					{bgok, JobID, Reply} ->
						% Call the function with the reply
						Fun(ok, Reply);
					{bgerror, JobID, Reply} ->
						Fun(error, Reply)
				end
		after ?TIMEOUT ->
			% send a timeout to the calling process
			Self ! {api, timeout}
		end
	end),

	% get the initial result of the command, NOT the asynchronous response, and
	% return it
	receive
		{api, X} -> X
	end.

%% @doc Request to receive any events in the list `List'.
event(Node, Events) when is_list(Events) ->
	{event, Node} ! list_to_tuple(lists:append([event], Events)),
	receive
		ok -> ok;
		{error, Reason} -> {error, Reason}
	after ?TIMEOUT ->
		timeout
	end;
event(Node, Event) when is_atom(Event) ->
	event(Node, [Event]).

session_event(Node, Events) when is_list(Events) ->
	{session_event, Node} ! list_to_tuple([session_event | Events]),
	receive
		ok -> ok;
		{error, Reason} -> {error, Reason}
	after ?TIMEOUT ->
			timeout
	end;
session_event(Node, Event) when is_atom(Event) ->
	session_event(Node, [Event]).

%% @doc Stop receiving any events in the list `Events' from `Node'.
nixevent(Node, Events) when is_list(Events) ->
	{nixevent, Node} ! list_to_tuple(lists:append([nixevent], Events)),
	receive
		ok -> ok;
		{error, Reason} -> {error, Reason}
	after ?TIMEOUT ->
		timeout
	end;
nixevent(Node, Event) when is_atom(Event) ->
	nixevent(Node, [Event]).

session_nixevent(Node, Events) when is_list(Events) ->
	{session_nixevent, Node} ! list_to_tuple([session_nixevent | Events]),
	receive
		ok -> ok;
		{error, Reason} -> {error, Reason}
	after ?TIMEOUT ->
		timeout
	end;
session_nixevent(Node, Event) when is_atom(Event) ->
	session_nixevent(Node, [Event]).

%% @doc Stop receiving any events from `Node'.
noevents(Node) ->
	{noevents, Node} ! noevents,
	receive
		ok -> ok;
		{error, Reason} -> {error, Reason}
	after ?TIMEOUT ->
		timeout
	end.

session_noevents(Node) ->
	{session_noevents, Node} ! session_noevents,
	receive
		ok -> ok;
		{error, Reason} -> {error, Reason}
	after ?TIMEOUT ->
			timeout
	end.

%% @doc Close the connection to `Node'.
close(Node) ->
	{close, Node} ! exit,
	receive
		ok -> ok
	after ?TIMEOUT ->
		timeout
	end.

%% @doc Send an event to FreeSWITCH. `EventName' is the name of the event and
%% `Headers' is a list of `{Key, Value}' string tuples. See the mod_event_socket
%% documentation for more information.
sendevent(Node, EventName, Headers) ->
	{sendevent, Node} ! {sendevent, EventName, Headers},
	receive
		ok -> ok;
		{error, Reason} -> {error, Reason}
	after ?TIMEOUT ->
		timeout
	end.

%% @doc Send a CUSTOM event to FreeSWITCH. `SubClassName' is the name of the event
%% subclass and `Headers' is a list of `{Key, Value}' string tuples. See the
%% mod_event_socket documentation for more information.
sendevent_custom(Node, SubClassName, Headers) ->
	{sendevent, Node} ! {sendevent, 'CUSTOM',  SubClassName, Headers},
	receive
		ok -> ok;
		{error, Reason} -> {error, Reason}
	after ?TIMEOUT ->
		timeout
	end.


%% @doc Send a message to the call identified by `UUID'. `Headers' is a list of
%% `{Key, Value}' string tuples.
sendmsg(Node, UUID, Headers) ->
	{sendmsg, Node} ! {sendmsg, UUID, Headers},
	receive
		ok -> ok;
		{error, Reason} -> {error, Reason}
	after ?TIMEOUT ->
		timeout
	end.


%% @doc Get the fake pid of the FreeSWITCH node at `Node'. This can be helpful
%% for linking to the process. Returns `{ok, Pid}' or `timeout'.
getpid(Node) ->
	{getpid, Node} ! getpid,
	receive
		{ok, Pid} when is_pid(Pid) -> {ok, Pid}
	after ?TIMEOUT ->
		timeout
	end.

%% @doc Request that FreeSWITCH send any events pertaining to call `UUID' to
%% `Process' where process is a registered process name.
handlecall(Node, UUID, Process) ->
	{handlecall, Node} ! {handlecall, UUID, Process},
	receive
		ok -> ok;
		{error, Reason} -> {error, Reason}
	after ?TIMEOUT ->
		timeout
	end.

%% @doc Request that FreeSWITCH send any events pertaining to call `UUID' to
%% the calling process.
handlecall(Node, UUID) ->
	{handlecall, Node} ! {handlecall, UUID},
	receive
		ok -> ok;
		{error, Reason} -> {error, Reason}
	after ?TIMEOUT ->
		timeout
	end.

%% @private
start_handler(Node, Type, Module, Function, State) ->
	Self = self(),
	spawn(fun() ->
		monitor_node(Node, true),
		{foo, Node} ! Type,
		receive
			ok ->
				Self ! {Type, {ok, self()}},
				apply(Module, Function, [Node, State]);
			{error,Reason} ->
				Self ! {Type, {error, Reason}}
		after ?TIMEOUT ->
				Self ! {Type, timeout}
		end
		end),
	
	receive
		{Type, X} -> X
	end.

%% @todo Notify the process if it gets replaced by a new log handler.

%% @doc Spawn `Module':`Function' as a log handler. The process will receive
%% messages of the form `{log, [{level, LogLevel}, {text_channel, TextChannel}, {file, FileName}, {func, FunctionName}, {line, LineNumber}, {data, LogMessage}]}'
%% or `{nodedown, Node}' if the FreesSWITCH node at `Node' exits.
%% 
%% The function specified by `Module':`Function' should be tail recursive and is
%% passed one argument; the name of the FreeSWITCH node.
%% 
%% Subsequent calls to this function for the same node replaces the
%% previous event handler with the newly spawned one.
%% 
%% This function returns either `{ok, Pid}' where `Pid' is the pid of the newly
%% spawned process, `{error, Reason}' or the atom `timeout' if FreeSWITCH did
%% not respond.
start_log_handler(Node, Module, Function, State) ->
	start_handler(Node, register_log_handler, Module, Function, State).

%% @todo Notify the process if it gets replaced with a new event handler.

%% @doc Spawn Module:Function as an event handler. The process will receive
%% messages of the form `{event, [UniqueID, {Key, Value}, {...}]}' where
%% `UniqueID' is either a FreeSWITCH call ID or `undefined' or
%% `{nodedown, Node}' if the FreeSWITCH node at `Node' exits. 
%% 
%% The function specified by `Module':`Function' should be tail recursive and is
%% passed one argument; the name of the FreeSWITCH node.
%% 
%% Subsequent calls to this function for the same node replaces the
%% previous event handler with the newly spawned one.
%% 
%% This function returns either `{ok, Pid}' where `Pid' is the pid of the newly
%% spawned process, `{error, Reason}' or the atom `timeout' if FreeSWITCH did
%% not respond.
start_event_handler(Node, Module, Function, State) ->
	start_handler(Node, register_event_handler, Module, Function, State).

%% @doc Spawn Module:Function as an XML config fetch handler for configs of type
%% `Section'. See the FreeSWITCH documentation for mod_xml_rpc for more
%% information on sections. The process will receive messages of the form 
%% `{fetch, Section, Tag, Key, Value, ID, Data}' or `{nodedown, Node}' if the
%% FreeSWITCH node at `Node' exits.
%% 
%% The function specified by `Module':`Function' should be tail recursive and is
%% passed one argument; the name of the FreeSWITCH node. The function should
%% send tuples back to FreeSWITCH of the form `{fetch_reply, ID, XML}' where
%%`ID' is the ID received in the request tuple and  `XML' is XML in string or
%% binary form of the form noted in the mod_xml_rpc documentation.
%%
%% Subsequent calls to this function for the same node and section will yield
%% undefined behaviour.
%% 
%% This function returns either `{ok, Pid}' where `Pid' is the pid of the newly
%% spawned process, `{error, Reason}' or the atom `timeout' if FreeSWITCH did
%% not respond.
start_fetch_handler(Node, Section, Module, Function, State) ->
	start_handler(Node, {bind, Section}, Module, Function, State).
