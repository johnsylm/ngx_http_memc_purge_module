#!/bin/sh

function rand()
{
    min=$1
    max=$(($2-$min+1))
    num=$(date +%s%N)
    echo $(($num%$max+$min))
}

DataProvider()
{
    GET_URLS=(
        "http://127.0.0.1/get_test.php?test=1"
        "http://127.0.0.1/get_test.php?test=2"
        "http://127.0.0.1/get_test.php?test=3"
        "http://127.0.0.1/get_test.php?test=4"
        "http://127.0.0.1/get_test.php?test=5"
    ) 
    POST_URLS=(
        "http://127.0.0.1/post_test.php -d test=1"
        "http://127.0.0.1/post_test.php -d test=2"
        "http://127.0.0.1/post_test.php -d test=3"
        "http://127.0.0.1/post_test.php -d test=4"
        "http://127.0.0.1/post_test.php -d test=5"
    )

    if [ "$1" == "get" ]; then
        echo ${GET_URLS[$2]}
    fi

    if [ "$1" == "post" ]; then
        echo ${POST_URLS[$2]}
    fi
}

Get() 
{
    ud=`DataProvider get ${1}`
    echo `curl -s --cookie "region=cn; lang=cn" ${ud}`
}

Post()
{
    ud=`DataProvider post ${1}`
    echo `curl -s ${ud}`
}
