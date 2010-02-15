#!/usr/local/bin/lua 
require("ESL") 

local command = arg[1];
table.remove(arg, 1);
local args = table.concat(arg, " ");

local con = ESL.ESLconnection("localhost", "8021", "ClueCon");
local e = con:api(command, args);
print(e:getBody());
