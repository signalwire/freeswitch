create table freeswitchcdr (
	callid bigint unsigned default 0 primary key, /* This will need to be handled specially for auto increment, as that might not be standard */
	callstartdate datetime NOT NULL,
	callanswerdate datetime NOT NULL,
	calltransferdate datetime NOT NULL,
	callenddate datetime NOT NULL,
	originated tinyint default 0,
	clid varchar(80) default "Freeswitch - Unknown",
	src varchar(80) NOT NULL,
	dst varchar(80) NOT NULL,
	ani varchar(80) default "",
	aniii varchar(80) default "",
	dialplan varchar(80) default "",
	myuuid char(36) NOT NULL,
	destuuid char(36) NOT NULL,
	srcchannel varchar(80) NOT NULL,
	dstchannel varchar(80) NOT NULL, /* Need to decide - this might be redundant as you can link the records via uuid */
	network_addr varchar(40) default "",
	lastapp varchar(80) default "",
	lastdata varchar(255) default "",
	billusec bigint default 0,
	disposition tinyint default 0, /* 0 = Busy or Unanswered, 1 = Answered */
	hangupcause int default 0,
	amaflags tinyint default 0
);

create index myuuid_index on freeswitchcdr (myuuid);
create index destuuid_index on freeswitchcdr (destuuid);

create table chanvars (
	callid bigint unsigned default 0,
	varname varchar(80) NOT NULL,
	varvalue varchar(255) default ""
);

create index callid_index on chanvars(callid,varname);
