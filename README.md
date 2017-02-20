Description
===========

An extension of Nginx, using the url as key.If key exists, return value from Memcache directly. 

Table of Contents
=================

* [Description](#description)
* [Configuration Options (nginx.conf)](#configuration-options-nginxconf)
* [Installation](#installation)
* [Usage](#usage)
* [PHP Tool](#php-tool)
* [Unit test](#unit-test)

Configuration Options (nginx.conf)
==================================

`All of the following options must be contained in location block.`
```nginx
#......
ngx_memc_purge_active on|off #Default:off , Control this module is open 
ngx_memc_purge_memcip 127.0.0.1 #Default:127.0.0.1
ngx_memc_purge_memcport 11211 #Default:11211
ngx_memc_purge_memcpre PLAYURL_ #Default:PLAYURL_ , Key prefix
#......
```

Installation
============

* Recommend version of nginx : 1.9.x / 1.10.3
* Dependent libraries: libmemcached , openssl-devel , zlib-devel

Take for example , install Nginx with this module on CentOS 7.1.

```bash

yum install -y openssl-devel zlib-devel

wget 'https://launchpad.net/libmemcached/1.0/1.0.18/+download/libmemcached-1.0.18.tar.gz'
tar -xzvf libmemcached-1.0.18.tar.gz
cd libmemcached-1.0.18
./configure --prefix=/usr
make
make install

wget 'http://nginx.org/download/nginx-1.9.0.tar.gz'
tar -xzvf nginx-1.9.0.tar.gz
cd nginx-1.9.0/
./configure --add-module=/path/to/ngx_http_memc_module
make
make install
```


Usage 
=====

* Example of nginx.conf
```nginx
http{
    #......
    server {
        listen       80;
        server_name  localhost 127.0.0.1;
        root /path/to/git/ngx_http_memc_purge_module/php-tool;
        location / {
            index  test.html test.htm;
        }
        location ~ \.php {
            ngx_memc_purge_active on; # Other options are using default
            fastcgi_pass   127.0.0.1:9000;
            fastcgi_param  SCRIPT_FILENAME  $document_root$fastcgi_script_name;
            include        fastcgi_params;
        }
    }
}
```

PHP Tool
========

This object contains a [php tool](https://github.com/johnsylm/ngx_http_memc_purge_module/tree/master/php-tool) that use url as key following the same rule of nginx , then write the key to Memcache and read it by nginx.<br />
Set the Memcache server IP/PORT in php , like the following codes:

```php
//......
<?php
class ngx_memc_purge
{
    //......
    
    private static $_memip = "127.0.0.1";
    private static $_memport = 11211;

    //......
}
```

Set the options of Memcache (ip/port) in nginx.conf according to the options in php.

Unit test
=========

* [Testing tool](https://github.com/johnsylm/ngx_http_memc_purge_module/tree/master/unit-test) base on shunit2.
* Configure Url DataProvider in : mp_request.sh:11

```bash
cd /path/to/git/ngx_http_memc_purge_module/unit-test
chmod u+x -R .
./mp_test.sh
```
