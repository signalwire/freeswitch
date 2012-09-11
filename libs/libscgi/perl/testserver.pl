use FSSCGI;


my $handle = new FSSCGI::SCGIhandle();

if ($handle->bind("127.0.0.1", 7777)) {

  while($handle->accept()) {
    print "REQ: " . $handle->getBody(). "\n\n";
    $handle->respond("W00t!!!!!!\n");
  }

  print "DONE\n";

} else {
  print "FAIL\n";
}
