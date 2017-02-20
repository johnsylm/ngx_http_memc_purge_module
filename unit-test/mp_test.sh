#!/bin/sh

:<<CARY_COMMENT
Unit test (shunit2) for ngx_http_memc_purge_module.
Author: Cary
Date: 2017/02/18
Time: 03:00
CARY_COMMENT

. ./mp_request.sh

testMadeByPHP()
{   
    result=`Get ${CG_rnd}`
    echo -e "Result of GET: ${result}"
    assertEquals "GET works! Arg: test=>${CG_rnd_val}.By PHP" "${result}"

    result=`Post ${CG_rnd}`
    echo -e "Result of POST: ${result}"
    assertEquals "POST works! Arg: test=>${CG_rnd_val}.By PHP" "${result}"

    echo -e
}

testMadeByNgx() 
{
    result=`Get ${CG_rnd}`
    echo -e "Result of GET: ${result}"
    assertEquals "GET works! Arg: test=>${CG_rnd_val}." "${result}"

    result=`Post ${CG_rnd}`
    echo -e "Result of POST: ${result}"
    assertEquals "POST works! Arg: test=>${CG_rnd_val}." "${result}"
}

oneTimeSetUp()
{
    echo -e "Test exists expect"
    echo -e "------------------------------------------------------"

    /usr/bin/expect -v

    if [ $? -ne 0 ]; then
        echo -e "expect was needed by this test!"
        exit 1
    fi
    
    /usr/bin/expect ./mp_memc_flush.sh

    if [ $? -ne 0 ]; then
        echo -e "Connect memcache for operation 'flush_all' failed !"
        exit 1
    fi

    CG_rnd=`rand 0 4`
    CG_rnd_val=`echo ${CG_rnd}+1|bc`

    geturl=`DataProvider get ${CG_rnd}`
    posturl=`DataProvider post ${CG_rnd}`

    echo -e
    echo -e "使用 php-tool 中的测试脚本 , 生成一个随机参数 URL"
    echo -e "------------------------------------------------------"
    echo -e "GET(curl) : ${geturl}"
    echo -e "POST(curl) : ${posturl}"
    echo -e "------------------------------------------------------"

    unset geturl posturl
}

. ./shunit2/shunit2
