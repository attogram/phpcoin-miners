<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Headers: *");

$node = $_GET['node'];
unset($_GET['node']);

$url = $node . $_SERVER['REQUEST_URI'];
$url = str_replace('proxy.php', '', $url);
$url = str_replace('&node=' . urlencode($node), '', $url);

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $data = file_get_contents('php://input');
    $options = [
        'http' => [
            'header'  => "Content-type: application/x-www-form-urlencoded\r\n",
            'method'  => 'POST',
            'content' => $data,
        ],
    ];
    $context  = stream_context_create($options);
    $result = file_get_contents($url, false, $context);
    echo $result;
} else {
    $result = file_get_contents($url);
    echo $result;
}
