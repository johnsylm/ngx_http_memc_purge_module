<?php
include __DIR__.'/ngx_memc_purge.php';

$output = "GET works! Arg: test=>{$_GET['test']}.";

// 如果请求方法是 GET , 那么 以下两个 cookie 是必须的
ngx_memc_purge::setDefaultCookie();
ngx_memc_purge::writePageToMem($output);

header("Content-type: text/html; charset=utf-8");
$output .= "By PHP";

echo($output);
