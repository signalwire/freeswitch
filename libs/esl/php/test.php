<?php
require_once('ESL.php');

$esl = new eslConnection('127.0.0.1', '8021', 'Synway');
$e = $esl->sendRecv("api status");
print $e->getBody();

?>
