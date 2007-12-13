CREATE TABLE `cdr` ( 
`caller_id_name` varchar(80) NOT NULL default '', 
`caller_id_number` varchar(80) NOT NULL default '', 
`destination_number` varchar(80) NOT NULL default '', 
`context` varchar(80) NOT NULL default '', 
`start_timestamp` datetime NOT NULL default '0000-00-00 00:00:00',  
`answer_timestamp` datetime NOT NULL default '0000-00-00 00:00:00', 
`end_timestamp` datetime NOT NULL default '0000-00-00 00:00:00', 
`duration` int(11) NOT NULL default '0', 
`billsec` int(11) NOT NULL default '0', 
`hangup_cause` varchar(45) NOT NULL default '',  
`uuid` varchar(36) NOT NULL default '',
`bleg_uuid` varchar(36) NOT NULL default '',
`accountcode` varchar(20) NOT NULL default ''
); 



