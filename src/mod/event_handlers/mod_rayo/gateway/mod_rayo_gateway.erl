%%
%% Copyright (c) 2013 Grasshopper
%%
%% Permission is hereby granted, free of charge, to any person obtaining a copy
%% of this software and associated documentation files (the "Software"), to deal
%% in the Software without restriction, including without limitation the rights
%% to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
%% copies of the Software, and to permit persons to whom the Software is
%% furnished to do so, subject to the following conditions:
%% 
%% The above copyright notice and this permission notice shall be included in
%% all copies or substantial portions of the Software.
%% 
%% THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
%% IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
%% FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
%% AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
%% LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
%% OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
%% THE SOFTWARE.
%%
%% Contributors:
%%   Chris Rienzo <chris.rienzo@grasshopper.com>
%%
%% Maintainer: Chris Rienzo <chris.rienzo@grasshopper.com>
%%
%% mod_rayo_gateway.erl -- ejabberd Rayo gateway module
%%
-module(mod_rayo_gateway).

-include("ejabberd.hrl").
-include("jlib.hrl").

-behavior(gen_server).
-behavior(gen_mod).

%%  JID mappings
%%
%%  Entity          Internal JID                     Mapped JID
%%  ======          ===============                  ===============
%%  Client          user@domain/resource             gateway@internal_domain/gw-resource
%%  Node            node_domain                      external_domain
%%  Call            uuid@node_domain                 node_domain|uuid@external_domain
%%  Call Resource   uuid@node_domain/resource        node_domain|uuid@external_domain/resource
%%  Mixer           name@node_domain                 node_domain|name@external_domain
%%  Mixer Resource  name@node_domain/resource        node_domain|name@external_domain/resource

%% TODO don't allow nodes to act as clients
%% TODO don't allow clients to act as nodes

-export([
	start_link/2,
	start/2,
	stop/1,
	init/1,
	handle_call/3,
	handle_cast/2,
	handle_info/2,
	terminate/2,
	code_change/3,
	route_internal/3,
	route_external/3
]).

-define(PROCNAME, ejabberd_mod_rayo_gateway).
-define(NS_RAYO, "urn:xmpp:rayo:1").
-define(NS_PING, "urn:xmpp:ping").

-record(rayo_config, {name, value}).
-record(rayo_clients, {jid, status}).
-record(rayo_nodes, {jid, status}).
-record(rayo_entities, {external_jid, internal_jid, dcp_jid, type}).

start_link(Host, Opts) ->
	Proc = gen_mod:get_module_proc(Host, ?PROCNAME),
	gen_server:start_link({local, Proc}, ?MODULE, [Host, Opts], []).

% Start the module process
start(Host, Opts) ->
	Proc = gen_mod:get_module_proc(Host, ?PROCNAME),
	ChildSpec = {Proc,
		{?MODULE, start_link, [Host, Opts]},
		temporary,
		1000,
		worker,
		[?MODULE]},
	supervisor:start_child(ejabberd_sup, ChildSpec).

% Shutdown the module
stop(Host) ->
	Proc = gen_mod:get_module_proc(Host, ?PROCNAME),
	gen_server:call(Proc, stop),
	supervisor:terminate_child(ejabberd_sup, Proc),
	supervisor:delete_child(ejabberd_sup, Proc).

% Initialize the module
init([Host, Opts]) ->
	?DEBUG("MOD_RAYO_GATEWAY: Starting", []),

	mnesia:delete_table(rayo_clients),
	mnesia:create_table(rayo_clients, [{attributes, record_info(fields, rayo_clients)}]),
	mnesia:delete_table(rayo_nodes),
	mnesia:create_table(rayo_nodes, [{attributes, record_info(fields, rayo_nodes)}]),
	mnesia:delete_table(rayo_entities),
	mnesia:create_table(rayo_entities, [{attributes, record_info(fields, rayo_entities)}, {index, [internal_jid]}]),
	mnesia:delete_table(rayo_config),
	mnesia:create_table(rayo_config, [{attributes, record_info(fields, rayo_config)}]),

	{A1,A2,A3} = now(),
	random:seed(A1, A2, A3),

	% create virtual domains
	InternalDomain = gen_mod:get_opt_host(Host, Opts, "rayo-int.@HOST@"),
	ExternalDomain = gen_mod:get_opt_host(Host, Opts, "rayo.@HOST@"),
	{ok, Hostname} = inet:gethostname(),
	InternalClient = "gateway@" ++ InternalDomain ++ "/" ++ Hostname ++ "-" ++ integer_to_list(random:uniform(65535)),
	?DEBUG("MOD_RAYO_GATEWAY: InternalDomain = ~p, ExternalDomain = ~p, InternalClient = ~p", [InternalDomain, ExternalDomain, InternalClient]),
	mnesia:transaction(
		fun() ->
			mnesia:write(#rayo_config{name = "internal_domain", value = InternalDomain}),
			mnesia:write(#rayo_config{name = "internal_client", value = InternalClient}),
			mnesia:write(#rayo_config{name = "external_domain", value = ExternalDomain})
		end
	),

	% set up routes to virtual domains
	ejabberd_router:register_route(InternalDomain, {apply, ?MODULE, route_internal}),
	ejabberd_router:register_route(ExternalDomain, {apply, ?MODULE, route_external}),
	{ok, Host}.

handle_call(stop, _From, Host) ->
	{stop, normal, ok, Host}.

handle_cast(_Msg, Host) ->
	{noreply, Host}.

handle_info(_Msg, Host) ->
	{noreply, Host}.

terminate(_Reason, Host) ->
	ejabberd_router:unregister_route(Host),
	ok.

code_change(_OldVsn, Host, _Extra) ->
	{ok, Host}.

register_rayo_node(Jid) ->
	Write = fun() ->
		mnesia:write(#rayo_nodes{jid = Jid, status = "online" })
	end,
	Result = mnesia:transaction(Write),
	?DEBUG("MOD_RAYO_GATEWAY: register node: ~p, result = ~p, ~p nodes total", [jlib:jid_to_string(Jid), Result, num_rayo_nodes()]),
	case num_clients() >= 1 of
		true ->
			ejabberd_router:route(internal_client(), Jid, online_presence());
		_ ->
			ok
	end,
	ok.

% TODO call this when s2s connection is dropped
unregister_rayo_node(Jid) ->
	Delete = fun() ->
		mnesia:delete({rayo_nodes, Jid})
	end,
	Result = mnesia:transaction(Delete),
	Size = mnesia:table_info(rayo_nodes, size),
	?DEBUG("MOD_RAYO_GATEWAY: unregister node: ~p, result = ~p, ~p nodes total", [jlib:jid_to_string(Jid), Result, Size]),
	ok.

% Add client
register_rayo_client(Jid) ->
	Write = fun() ->
		mnesia:write(#rayo_clients{jid = Jid, status = "online" })
	end,
	Result = mnesia:transaction(Write),
	Size = num_clients(),
	?DEBUG("MOD_RAYO_GATEWAY: register client: ~p, result = ~p, ~p clients total", [jlib:jid_to_string(Jid), Result, Size]),
	case Size of
		1 ->
			route_to_list(internal_client(), all_rayo_nodes(), online_presence());
		_ ->
			ok
	end,
	ok.

% Remove client
% TODO call this when c2s connection is dropped
unregister_rayo_client(Jid) ->
	Delete = fun() ->
		mnesia:delete({rayo_clients, Jid})
	end,
	Result = mnesia:transaction(Delete),
	Size = num_clients(),
	?DEBUG("MOD_RAYO_GATEWAY: unregister client: ~p, result = ~p, ~p clients total", [jlib:jid_to_string(Jid), Result, Size]),
	case Size of
		0 ->
			route_to_list(internal_client(), all_rayo_nodes(), offline_presence());
		_ ->
			ok
	end,
	ok.

% Add node entity
register_rayo_node_entity(ExtJid, IntJid, DcpJid, Type) ->
	Write = fun() ->
		mnesia:write(#rayo_entities{external_jid = ExtJid, internal_jid = IntJid, dcp_jid = DcpJid, type = Type})
	end,
	Result = mnesia:transaction(Write),
	Size = mnesia:table_info(rayo_entities, size),
	?DEBUG("MOD_RAYO_GATEWAY: register entity: ~p, result = ~p, ~p entities total", [jlib:jid_to_string(ExtJid), Result, Size]),
	ok.

% Remove node entity
unregister_rayo_node_entity(ExtJid) ->
	Delete = fun() ->
		mnesia:delete({rayo_entities, ExtJid})
	end,
	Result = mnesia:transaction(Delete),
	Size = mnesia:table_info(rayo_entities, size),
	?DEBUG("MOD_RAYO_GATEWAY: unregister entity: ~p, result = ~p, ~p entities total", [jlib:jid_to_string(ExtJid), Result, Size]),
	ok.

% find node entity given enitity's (or its component's) internal JID
find_rayo_node_entity_by_int_jid(IntJid) ->
	% remove resource from JID to find component's parent call/mixer
	case mnesia:dirty_index_read(rayo_entities, jlib:jid_remove_resource(IntJid), #rayo_entities.internal_jid) of
		[Entity | _] ->
			Entity;
		_ ->
			none
	end.

% find node entity given enitity's (or its component's) external JID
find_rayo_node_entity_by_ext_jid(ExtJid) ->
	% remove resource from JID to find component's parent call/mixer
	case mnesia:dirty_read(rayo_entities, jlib:jid_remove_resource(ExtJid)) of
		[Entity | _] ->
			Entity;
		_ ->
			none
	end.

% find entity Definitive Controlling Party JID given entity external JID
find_rayo_node_entity_dcp_by_ext_jid(ExtJid) ->
	case find_rayo_node_entity_by_ext_jid(ExtJid) of
		{rayo_entities, _, _, DcpJid, _} ->
			DcpJid;
		_ ->
			none
	end.

% find entity Definitive Controlling Party JID given entity internal JID
find_rayo_node_entity_dcp_by_int_jid(IntJid) ->
	case find_rayo_node_entity_by_int_jid(IntJid) of
		{rayo_entities, _, _, DcpJid, _} ->
			DcpJid;
		_ ->
			none
	end.

% create External JID from Internal JID
% intnode@intdomain/resource -> intdomain-intnode@extdomain/resource
create_external_jid({jid, Node, Domain, Resource, _, _, _}) ->
	jlib:make_jid(Domain ++ "|" ++ Node, jlib:jid_to_string(external_domain()), Resource).

% create Internal JID from External JID
% intdomain-intnode@extdomain/resource -> intnode@intdomain/resource
create_internal_jid({jid, Node, _Domain, Resource, _, _, _}) ->
	% TODO use rayo_entities to lookup node... it's safer
	Idx = string:str(Node, "|"),
	case Idx > 0 of
		true ->
			jlib:make_jid(string:substr(Node, Idx + 1), string:substr(Node, 1, Idx - 1), Resource);
		false ->
			none
	end.

% Take control of entity
% Return {true, internal entity JID} if successful
set_entity_dcp(PcpJid, EntityJid) ->
	SetDcp = fun() ->
		case mnesia:wread(rayo_entities, EntityJid) of
			[{rayo_entities, EntityJid, InternalJid, none, Type}] ->
				% take control
				case mnesia:write(#rayo_entities{external_jid = EntityJid, internal_jid = InternalJid, dcp_jid = PcpJid, type = Type}) of
					ok ->
						{true, InternalJid};
					Else ->
						{error, Else}
				end;
			_ ->
				{false, []}
		end
	end,
	{_, Result} = mnesia:transaction(SetDcp),
	Result.

% Check if PCP has control of entity
% Return {true, internal entity JID} if true
is_entity_dcp(PcpJid, EntityJid) ->
	% quick check first
	case mnesia:dirty_read(rayo_entities, EntityJid) of
		[{rayo_entities, EntityJid, _, none, _}] ->
			% take control
			set_entity_dcp(PcpJid, EntityJid);
		[{rayo_entities, EntityJid, InternalJid, PcpJid, _}] ->
			{true, InternalJid};
		[{rayo_entities, EntityJid, InternalJid, _, _}] ->
			{false, InternalJid};
		[] ->
			?DEBUG("MOD_RAYO_GATEWAY: no match for EntityJid ~p", [EntityJid]),
			{false, none}
	end.

% Handle presence to external domain
route_external(From, {jid, [], _Domain, [], [], _LDomain, []} = To, {xmlelement, "presence", _Attrs, _Els} = Presence) ->
	?DEBUG("MOD_RAYO_GATEWAY: got client presence ~n~p", [Presence]),
	route_client_presence(From, To, Presence),
	ok;

% Handle presence to external domain resource
route_external(From, To, {xmlelement, "presence", _Attrs, _Els} = Presence) ->
	?DEBUG("MOD_RAYO_GATEWAY: got client presence to mixer ~n~p", [Presence]),
	% TODO check if actually being sent to mixer...
	route_client_presence_to_mixer(From, To, Presence),
	ok;

% Handle <message> to external domain
route_external(_From, _To, {xmlelement, "message", _Attrs, _Els} = Message) ->
	% ignore
	?DEBUG("MOD_RAYO_GATEWAY: got client message ~n~p", [Message]),
	ok;

% Handle <iq> to external domain
route_external(From, {jid, [], _Domain, [], [], _LDomain, []} = To, {xmlelement, "iq", _Attrs, _Els} = IQ) ->
	?DEBUG("MOD_RAYO_GATEWAY: got client iq to gateway ~n~p", [IQ]),
	case get_attribute_as_list(IQ, "type", "") of
		"get" ->
			case get_element(IQ, ?NS_PING, "ping") of
				undefined ->
					route_error_reply(To, From, IQ, ?ERR_BAD_REQUEST);
				_ ->
					route_result_reply(To, From, IQ)
			end;
		"set" ->
			case get_element(IQ, ?NS_RAYO, "dial") of
				undefined->
					route_error_reply(To, From, IQ, ?ERR_BAD_REQUEST);
				_ ->
					route_dial_call(To, From, IQ)
			end;
		"" ->
			route_error_reply(To, From, IQ, ?ERR_BAD_REQUEST)
	end,
	ok;

% Handle <iq> to external domain resource
route_external(From, To, {xmlelement, "iq", _Attrs, _Els} = IQ) ->
	?DEBUG("MOD_RAYO_GATEWAY: got client iq ~n~p", [IQ]),
	case is_entity_dcp(From, To) of
		{true, _} ->
			IntFrom = internal_client(),
			IntTo = create_internal_jid(To),
			route_iq_request(IntFrom, IntTo, IQ, fun(IQReply) -> route_iq_response(From, To, IQ, IQReply) end);
		{false, _} ->
			route_error_reply(To, From, IQ, ?ERR_CONFLICT);
		_ ->
			route_error_reply(To, From, IQ, ?ERR_BAD_REQUEST)
	end,
	ok.

% Handle <presence> to internal domain
route_internal(From, {jid, [], _Domain, [], [], _LDomain, []} = To, {xmlelement, "presence", _Attrs, _Els} = Presence) ->
	?DEBUG("MOD_RAYO_GATEWAY: got node presence to internal domain ~n~p", [Presence]),
	route_server_presence(From, To, Presence),
	ok;

% Handle <presence> to internal domain resource
route_internal(From, To, {xmlelement, "presence", _Attrs, _Els} = Presence) ->
	?DEBUG("MOD_RAYO_GATEWAY: got node presence to internal domain ~n~p", [Presence]),
	case To =:= internal_client() of
		true ->
			route_server_presence(From, To, Presence);
		false ->
			% TODO implement
			ok
	end,
	ok;

% Handle <message> to internal domain
route_internal(_From, _To, {xmlelement, "message", _Attrs, _Els} = Message) ->
	?DEBUG("MOD_RAYO_GATEWAY: got node message ~n~p", [Message]),
	% ignore
	ok;

% Handle <iq> to internal domain.
route_internal(From, {jid, [], _Domain, [], [], _LDomain, []} = To, {xmlelement, "iq", _Attrs, _Els} = IQ) ->
	?DEBUG("MOD_RAYO_GATEWAY: got node iq ~n~p", [IQ]),
	case get_attribute_as_list(IQ, "type", "") of
		"get" ->
			case get_element(IQ, ?NS_PING, "ping") of
				undefined ->
					route_error_reply(To, From, IQ, ?ERR_BAD_REQUEST);
				_ ->
					route_result_reply(To, From, IQ)
			end;
		"result" ->
			ejabberd_local:process_iq_reply(From, To, jlib:iq_query_or_response_info(IQ));
		"error" ->
			ejabberd_local:process_iq_reply(From, To, jlib:iq_query_or_response_info(IQ));
		"" ->
			% don't allow get/set from nodes
			route_error_reply(To, From, IQ, ?ERR_BAD_REQUEST)
	end,
	ok;

% Handle <iq> to internal domain resource.
route_internal(From, To, {xmlelement, "iq", _Attrs, _Els} = IQ) ->
	?DEBUG("MOD_RAYO_GATEWAY: got node iq ~n~p", [IQ]),
	case get_attribute_as_list(IQ, "type", "") of
		"result" ->
			ejabberd_local:process_iq_reply(From, To, jlib:iq_query_or_response_info(IQ));
		"error" ->
			ejabberd_local:process_iq_reply(From, To, jlib:iq_query_or_response_info(IQ));
		_ ->
			% Don't allow get/set from nodes
			route_error_reply(To, From, IQ, ?ERR_BAD_REQUEST)
	end,
	ok.

% Process presence message from rayo node
route_rayo_node_presence(From, _To, Presence) ->
	case get_attribute_as_list(Presence, "type", "") of
		"" ->
			case get_element(Presence, "show") of
				undefined ->
					?DEBUG("MOD_RAYO_GATEWAY: ignoring empty presence", []);
				Show ->
					case get_cdata_as_list(Show) of
						"chat" ->
							register_rayo_node(From);
						"dnd" ->
							unregister_rayo_node(From);
						"xa" ->
							unregister_rayo_node(From);
						"" ->
							unregister_rayo_node(From)
					end
			end;
		"unavailable" ->
			%TODO broadcast end instead?
			unregister_rayo_node(From)
	end,
	ok.

% Process presence from call
route_call_presence(From, _To, Presence) ->
	%TODO join/unjoin mixer events
	case get_attribute_as_list(Presence, "type", "") of
		"" ->
			case get_element(Presence, ?NS_RAYO, "offer") of
				undefined ->
					route_rayo_entity_stanza(From, Presence);
				_ ->
					route_offer_call(From, Presence)
			end;
		"unavailable" ->
			case get_element(Presence, ?NS_RAYO, "end") of
				undefined ->
					route_rayo_entity_stanza(From, Presence);
				_ ->
					route_rayo_entity_stanza(From, Presence),
					unregister_rayo_node_entity(create_external_jid(From))
			end
	end,
	ok.

% presence from node
route_server_presence({jid, [], _Domain, [], [], _LDomain, []} = From, To, Presence) ->
	route_rayo_node_presence(From, To, Presence),
	ok;

% presence from call/mixer
route_server_presence(From, To, Presence) ->
	% TODO mixer
	route_call_presence(From, To, Presence),
	ok.

% presence from Rayo Client
route_client_presence(From, _To, Presence) ->
	case get_attribute_as_list(Presence, "type", "") of
		"" ->
			case get_element(Presence, "show") of
				undefined ->
					?DEBUG("MOD_RAYO_GATEWAY: ignoring empty presence", []);
				Show ->
					case get_cdata_as_list(Show) of
						"chat" ->
							register_rayo_client(From);
						"dnd" ->
							unregister_rayo_client(From);
						_ ->
							unregister_rayo_client(From)
					end
			end;
		"unavailable" ->
			unregister_rayo_client(From);
		_ ->
			ok
	end,
	ok.

% route client directed presence to mixer
route_client_presence_to_mixer(_From, _To, _Presence) ->
	% TODO
	ok.

% Handle offer to client
route_offer_call(From, Offer) ->
	% Any clients available?
	case pick_client() of
		none ->
			% TODO reject?
			ok;
		ClientDcp ->
			% Remember call
			ExtFrom = create_external_jid(From),
			register_rayo_node_entity(ExtFrom, From, ClientDcp, call),
			ejabberd_router:route(ExtFrom, ClientDcp, Offer)
	end,
	ok.

% convert URI to a JID
uri_to_jid(Uri) ->
	JidString = case string:str(Uri, "xmpp:") of
		1 ->
			string:substr(Uri, 6);
		_ ->
			Uri
	end,
	jlib:string_to_jid(JidString).

% convert internal IQ reply to an external reply
create_external_iq_reply(OrigIQ, {xmlelement, _, _, Els} = IQReply) ->
	IQId = get_attribute_as_list(OrigIQ, "id", ""),
	IQType = get_attribute_as_list(IQReply, "type", ""),
	{xmlelement, "iq", [{"id", IQId}, {"type", IQType}], Els}.

% Process dial response
route_dial_call_response(OrigFrom, OrigTo, OrigIQ, timeout) ->
	% TODO retry on different node?
	route_iq_response(OrigFrom, OrigTo, OrigIQ, timeout);

route_dial_call_response(OrigFrom, OrigTo, OrigIQ, IQReply) ->
	?DEBUG("MOD_RAYO_GATEWAY: IQ response for ~p", [OrigIQ]),
	IQReplyPacket = jlib:iq_to_xml(IQReply),
	case get_element(IQReplyPacket, "error") of
		undefined ->
			case get_element(IQReplyPacket, "ref") of
				undefined ->
					ok;
				Ref ->
					IntJid = uri_to_jid(get_attribute_as_list(Ref, "uri", "")),
					register_rayo_node_entity(create_external_jid(IntJid), IntJid, OrigFrom, call)
			end;
		_ ->
			ok
	end,
	ejabberd_router:route(OrigTo, OrigFrom, create_external_iq_reply(OrigIQ, IQReplyPacket)),
	ok.

% Forward dial to node
route_dial_call(From, To, Dial) ->
	% any nodes available?
	case num_rayo_nodes() > 0 of
		true ->
			IntFrom = internal_client(),
			case pick_rayo_node() of
				none ->
					route_error_reply(To, From, Dial, ?ERR_SERVICE_UNAVAILABLE);
				NodeJid ->
					route_iq_request(IntFrom, NodeJid, Dial, fun(IQReply) -> route_dial_call_response(From, To, Dial, IQReply) end)
			end;
		_ ->
			route_error_reply(To, From, Dial, ?ERR_RESOURCE_CONSTRAINT)
	end.

% return configuration value given name
config_value(Name) ->
	case catch mnesia:dirty_read(rayo_config, Name) of
		[{rayo_config, Name, Value}] ->
			Value;
		_ ->
			""
	end.

% return internal client name
internal_client() ->
	jlib:string_to_jid(config_value("internal_client")).

% return internal domain name
internal_domain() ->
	jlib:string_to_jid(config_value("internal_domain")).

% return external domain name
external_domain() ->
	jlib:string_to_jid(config_value("external_domain")).

% return number of registered clients
num_clients() ->
	mnesia:table_info(rayo_clients, size).

% return all registered client JIDs
all_clients() ->
	case mnesia:transaction(fun() -> mnesia:all_keys(rayo_clients) end) of
		{atomic, Keys} ->
			Keys;
		_ ->
			[]
	end.

% pick a registered client
pick_client() ->
	% pick at random for now...
	case all_clients() of
		[] ->
			none;
		AllClients ->
			lists:nth(random:uniform(length(AllClients)), AllClients)
	end.

% pick a registered node
pick_rayo_node() ->
	% pick at random for now...
	case all_rayo_nodes() of
		[] ->
			none;
		AllNodes ->
			lists:nth(random:uniform(length(AllNodes)), AllNodes)
	end.

% return number of registered rayo nodes
num_rayo_nodes() ->
	mnesia:table_info(rayo_nodes, size).

% return all rayo node JIDs
all_rayo_nodes() ->
	case mnesia:transaction(fun() -> mnesia:all_keys(rayo_nodes) end) of
		{atomic, Keys} ->
			Keys;
		_ ->
			[]
	end.

presence(Status) ->
	{xmlelement, "presence", [], [
		{xmlelement, "show", [], [
			{xmlcdata, Status}
		]}
	]}.

online_presence() ->
	presence(<<"chat">>).

offline_presence() ->
	presence(<<"dnd">>).

route_to_list(From, ToList, Stanza) ->
	lists:map(fun(To) -> ejabberd_router:route(From, To, Stanza) end, ToList),
	ok.

% route stanza from entity
route_rayo_entity_stanza(From, Stanza) ->
	case find_rayo_node_entity_dcp_by_int_jid(From) of
		none ->
			?DEBUG("MOD_RAYO_GATEWAY: Failed to find DCP for ~p", [From]),
			ok;
		DcpJid ->
 			ejabberd_router:route(create_external_jid(From), DcpJid, Stanza)
 	end,
	ok.

% route IQ response from node to client
route_iq_response(OrigFrom, OrigTo, OrigIQ, timeout) ->
	route_error_reply(OrigTo, OrigFrom, OrigIQ, ?ERR_REMOTE_SERVER_TIMEOUT),
	ok;

route_iq_response(OrigFrom, OrigTo, OrigIQ, IQReply) ->
	?DEBUG("MOD_RAYO_GATEWAY: IQ response for ~p", [OrigIQ]),
	ejabberd_router:route(OrigTo, OrigFrom, create_external_iq_reply(OrigIQ, jlib:iq_to_xml(IQReply))),
	ok.

% route IQ from client to node
route_iq_request(From, To, {xmlelement, "iq", _Atts, Els}, ResponseCallback) ->
	ejabberd_local:route_iq(From, To, #iq{type = set, sub_el = Els}, ResponseCallback),
	ok.

% route IQ error given request
route_error_reply(From, To, IQ, Reason) ->
	ejabberd_router:route(From, To, jlib:make_error_reply(IQ, Reason)),
	ok.

% route IQ result given request
route_result_reply(From, To, IQ) ->
	ejabberd_router:route(From, To, jlib:make_result_iq_reply(IQ)),
	ok.

% XML parsing helpers

get_element(Element, Name) ->
	case xml:get_subtag(Element, Name) of
		false ->
			undefined;
		Subtag ->
			Subtag
	end.

get_element(Element, NS, Name) ->
	case get_element(Element, Name) of
		undefined ->
			undefined;
		Subtag ->
			case get_attribute_as_list(Subtag, "xmlns", "") of
				"" ->
					undefined;
				NS ->
					Subtag
			end
	end.

get_cdata_as_list(undefined) ->
	"";

get_cdata_as_list(Element) ->
	xml:get_tag_cdata(Element).

get_element_cdata_as_list(Element, Name) ->
	get_cdata_as_list(get_element(Element, Name)).

get_element_cdata_as_list(Element, NS, Name) ->
	get_cdata_as_list(get_element(Element, NS, Name)).

get_element_attribute_as_list(Element, Name, AttrName, Default) ->
	get_attribute_as_list(get_element(Element, Name), AttrName, Default).

get_element_attribute_as_list(Element, NS, Name, AttrName, Default) ->
	get_attribute_as_list(get_element(Element, NS, Name), AttrName, Default).

get_attribute_as_list(undefined, _Name, _Default) ->
	undefined;

get_attribute_as_list(Element, Name, Default) ->
	case xml:get_tag_attr_s(Name, Element) of
		"" ->
			Default;
		Attr ->
			Attr
	end.
