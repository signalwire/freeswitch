# FreeSWITCH Custom GDB commands
define list_sessions
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