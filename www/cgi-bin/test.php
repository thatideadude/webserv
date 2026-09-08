#!/usr/bin/php-cgi
<?php

echo "Content-Type: text/html\r\n";
echo "\r\n";

echo "<html><body>";
echo "<h1>PHP CGI Test</h1>";
echo "<p>PHP is working through CGI!</p>";
echo "<p>Request Method: " .
     htmlspecialchars($_SERVER['REQUEST_METHOD'] ?? '', ENT_QUOTES, 'UTF-8') .
     "</p>";
echo "</body></html>";

?>
