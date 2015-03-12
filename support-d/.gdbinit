handle SIGPIPE pass noprint nostop
handle SIGTTIN pass noprint nostop

# FreeSWITCH Custom GDB commands
define list_sessions
	dont-repeat
	printf "Listing sessions: \n"
	set $i = 0
	set $idx = 0
	set $len = session_manager.session_table->tablelength
	while($idx < $len)
	  set $x = session_manager.session_table->table[$idx]
	  while($x != 0x0)
	    printf "uuid %s is at %p\n", $x->k, $x->v
	    set $i = $i + 1
	    set $x = $x->next
	  end
	  set $idx = $idx + 1

	end
	printf "Found %d sessions.\n", $i
end
document list_sessions
Print a list of session uuid and pointers
end

define hash_it_str
	dont-repeat
	set $i = 0
	set $idx = 0
	set $len = $arg0->tablelength
	printf "len: %d\n", $arg0->tablelength

	while($idx < $len)
	  set $x = $arg0->table[$idx]
	  while($x != 0x0)
	    printf "key: %s valueptr: %p\n", $x->k, $x->v
	    set $x = $x->next
	    set $i = $i + 1
	  end
	  set $idx = $idx + 1
	end
end
document hash_it_str 
Usage: hash_it_str [hashtable]
Prints the content of a hashtable displaying the key as a string and the value as pointer
end


define hash_it_str_x
	dont-repeat
	set $i = 0
	set $idx = 0
	set $len = $arg0->tablelength
	while($idx < $len)
	  set $x=$arg0->table->[$idx]
	  while($x != 0x0)
	    printf "key: %s\n", $x->k
	    print (($arg1*)$x->v)->$arg2
	    printf "\n\n"
	    set $x = $x->next
	    set $i = $i + 1
	  end
	end
end
document hash_it_str_x
Usage: hash_it_str_x [hashtable] [value_type] [member]
Prints the content of a hashtable displaying the key as a string and a specific member of the value struct. Args: hashtable value_type member
end

define event_dump
	dont-repeat
	set $x = $arg0->headers
	while($x != 0x0)
		printf "%s = %s\n", $x->name, $x->value
		set $x = $x->next
	end
end
document event_dump
Usage: event_dump [switch_event_t*]
Print an event's headers and values
end

define print_list
	dont-repeat
	set $x = $arg0
	while ($x != 0x0)
		print *$x
		set $x = $x->next
	end
end
document print_list
Usage print_list [symbol]
Prints all the remaining elements of a linked list
end

define print_tags
	dont-repeat
	set $x = $arg0
	while (*((int*)$x) != 0x0)
		info sym $x->t_tag
		printf "%p \"%s\"\n", $x->t_value, $x->t_value
		set $x = $x + 1
	end
end
document print_tags
Usage print_tags [tags]
List sofia tags and their values
end

define setup_session
	set $session=(switch_core_session_t*)$arg0
	set $channel = $session->channel
	printf "UUID: %s\nName: %s\nState: %d\n", $session->uuid_str, $channel->name, $channel->state
end
document setup_session
Usage setup_session [session address]
Sets session and channel from the given address
end

define setup_sofia
	set $tech_pvt = (private_object_t*)$session->private_info
	set $nh = $tech_pvt->nh
end
document setup_sofia
No arguments. Sets nh and tech_pvt from the current session
end
