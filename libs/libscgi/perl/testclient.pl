use FSSCGI;


my $handle = new FSSCGI::SCGIhandle();

        $handle->addParam( "REQUEST_METHOD", "POST");
        $handle->addParam( "REQUEST_URI", "/deepthought");
        $handle->addParam( "TESTING", "TRUE");
        $handle->addParam( "TESTING", "TRUE");
        $handle->addBody("What is the answer to life?");



if ((my $response = $handle->sendRequest("127.0.0.1", 7777, 10000))) {
  print "RESP[$response]\n";
} else {
  print "ERROR!\n";
}

