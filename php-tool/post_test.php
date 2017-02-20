<?php
include __DIR__.'/ngx_memc_purge.php';

$output = "POST works! Arg: test=>{$_POST['test']}.";
ngx_memc_purge::writePageToMem($output);

header("Content-type: text/html; charset=utf-8");
$output .= "By PHP";

echo($output);
