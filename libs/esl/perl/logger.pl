use ESL;

my $host = "localhost";
my $port = "8021";
my $pass = "ClueCon";
my $profile = "internal";
my $file = "";
my $debug = "";
my $paste = "";
my $i;
my $argc = @ARGV;
my $e;
my $running = 1;
my $con;

my $USAGE = "
FreeSWITCH Logger Utility

USAGE:
-h --help                   This help
-H --host                   Choose host
-p --port <port>            Choose port
-P -pass  <pass>            Choose password
-f --file <file>            Output file
-pb --paste-bin <name>	    Post to FreeSWITCH Paste Bin
-sp --sip-profiles <list>   List of SIP profiles to trace
-sd --sip-debug <level>     Set SIP debug level

No arguments given will trace profile 'internal' to STDOUT
";

$SIG{INT} = sub { $running = 0 };

sub parse(\$\$$) {
    my ($index, $ref, $regex) = @_;

    if ($ARGV[$$index] =~ $regex) {
	die "missing arg!" if (!$ARGV[$$index+1]);
	$$ref = $ARGV[++$$index];
	return 1;
    }
    return 0;
}

for($i = 0; $i < $argc; $i++) {
    if ($ARGV[$i] =~ /^\-h$|^\-\-help$/) {
	print $USAGE;
	exit;
    }

    if (! (parse($i, $host, '^-H$|^--host$') ||
	   parse($i, $port, '^-p$|^--port$') ||
	   parse($i, $pass, '^-P$|^--pass$') ||
	   parse($i, $file, '^-f$|^--file$') ||
	   parse($i, $paste, '^-pb$|^--paste-bin$') ||
	   parse($i, $profile, '^-sp$|^--sip-profile$') ||
	   parse($i, $debug, '^-sd$|^--sip-debug$')
	   )) {
	die "invalid arg!";
    }
}

if ($paste) {
    if (!$file) {
	$file = "./logger_post.log";
    }
}

if ($file) {
    open (F, ">$file");
    select F;
}

if ($paste) {
    print F "paste=Send&remember=0&poster=${paste}&format=fslog&code2=";
}

$con = new ESL::ESLconnection($host, $port, $pass);

sub do_api($) {
    my ($cmd, $args) = split(" ", $_[0], 2);
    my $e = $con->api($cmd, $args);
    if ($e) {
	print STDERR $e->getBody() . "\n";
    }
}

foreach (split(",", $profile)) {
    do_api("sofia profile $_ siptrace on");
}

if ($debug) {
    do_api("sofia loglevel all $debug");
}

$e = $con->sendRecv("log 7");
print STDERR $e->getBody() . "\n" if ($e);

while($con->connected() && $running) {
    $e = $con->recvEventTimed(100);
    if ($e and $e->getHeader("content-type") eq "log/data") {
	print $e->getBody();
    }
}
print STDERR "Stopping\n";

foreach (split(",", $profile)) {
    do_api("sofia profile $_ siptrace off");
}

if ($debug) {
    do_api("sofia loglevel all 0");
}

$e = $con->sendRecv("log 4");
print STDERR $e->getBody() . "\n" if ($e);

print STDERR "Done.....\n";

if ($file) {
    close F;
    print STDERR "Data written to $file\n";
}

if ($paste) {
    my $path;
    system("mkdir -p .fs_logger");
    chdir(".fs_logger") or die "I/O Error!";
    
    if ($file =~ /^\/.*/) {
	$path = $file;
    } else {
	$path = "../$file";
    }
    
    print STDERR "Posting to pastebin, please wait...\n";

    system("wget --output-file=/dev/null --http-user=pastebin --http-password=freeswitch http://pastebin.freeswitch.org --post-file=$path");
    $pb = `ls [0-9]*`;
    print STDERR "Data posted to pastebin [$paste] http://pastebin.freeswitch.org/$pb\n";
    chdir("..") or die "I/O Error!";
    system("rm -fr .fs_logger");
}
