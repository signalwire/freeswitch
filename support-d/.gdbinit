# FreeSWITCH Custom GDB commands
define list_sessions
	dont-repeat
	printf "Listing sessions: \n"
	set $i = 0
	set $x=session_manager.session_table->table->first
	while($x != 0x0)
		printf "uuid %s is at %p\n", $x->pKey, $x->data
		set $x = $x->next
		set $i = $i + 1
	end
	printf "Found %d sessions.\n", $i
end
document list_sessions Print a list of session uuid and pointers

define hash_it_str
	dont-repeat
	set $i = 0
	set $x=$arg0->table->first
	while($x != 0x0)
		printf "key: %s valueptr: %p\n", $x->pKey, $x->data
		set $x = $x->next
		set $i = $i + 1
	end
end
document hash_it_str Prints the content of a hashtable displaying the key as a string and the value as pointer

define hash_it_int
	dont-repeat
	set $i = 0
	set $x=$arg0->table->first
	while($x != 0x0)
		printf "key: %d valueptr: %p\n", $x->pKey, $x->data
		set $x = $x->next
		set $i = $i + 1
	end
end
document hash_it_int Prints the content of a hashtable displaying the key as an int and the value as pointer

define hash_it_str_x
	dont-repeat
	set $i = 0
	set $x=$arg0->table->first
	while($x != 0x0)
		printf "key: %s\n", $x->pKey
		print (($arg1)$x->data)->$arg2
		printf "\n\n"
		set $x = $x->next
		set $i = $i + 1
		end
end
document hash_it_str_x Prints the content of a hashtable displaying the key as a string and a specific member of the value struct. Args: hashtable value_type member

define hash_it_int_x
	dont-repeat
	set $i = 0
	set $x=$arg0->table->first
	while($x != 0x0)
		printf "key: %d\n", $x->pKey
		print (($arg1)$x->data)->$arg2
		printf "\n\n"
		set $x = $x->next
		set $i = $i + 1
		end
end
document hash_it_int_x Prints the content of a hashtable displaying the key as a string and a specific member of the value struct. Args: hashtable value_type member
