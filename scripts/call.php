<?php
require_once('ESL.php');
$my_company_DID = '2135551212';
$my_company_domain = 'mydomain.com';

$groups = array('default'
    , 'support'
    , 'billing'
    , 'sales'
);


$esl = new ESLconnection('127.0.0.1', '8021', 'ClueCon');
if (is_array($_REQUEST) && !empty($_REQUEST['callee'])) {
    $callee = str_replace(array('.', '(', ')', '-', ' '), '', $_REQUEST['callee']);
    $callee = ereg_replace('^(1|\+1)?([2-9][0-9]{2}[2-9][0-9]{6})$', '1\2', $callee);
    $group = !empty($_REQUEST['group']) ? $_REQUEST['group'] : 'default';
    $command_string = "api originate GROUP/$group &transfer($callee LCR)";
    echo $command_string;
    $res = $esl->sendRecv($command_string);
} else {
    echo "<form><br>\n";
    echo "Your Number: <input name=\"callee\"><br>\n";
    echo "Department: <select name=\"group\">";
    foreach ($groups as $group) {
        echo "<option value=\"$group\">$group</option>";
    }
    echo "</select>";
    echo "<input type=\"submit\" value=\"Call Me\">";
    echo "</form>";
}
