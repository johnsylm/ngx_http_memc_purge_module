<?php
/**
 * Author: Cary
 * Date: 17/02/17
 * Time: 20:00
 * Desc: 利用页面 URL 和 ngx_http_memc_purge_module 约定拼接方法 , 此方法 SET KEY 让扩展读.
 */
class ngx_memc_purge
{
    private static $_mem = null;
    private static $_memip = "127.0.0.1";
    private static $_memport = 11211;

    /**
     * 获取 memcache 连接实例
     * @param string $op 一个字符串型的 flag , 根据该 flag 决定 获取(建立) 或 销毁(关闭) Mem 实例
     * @return Instance of \Memcache|null
     */
    public static function memInstance($op = "open")
    {
        if($op === "open") {
            if (!(self::$_mem instanceof \Memcache)) {
                self::$_mem = new \Memcache();
                $isConnected = @self::$_mem->connect(self::$_memip, 
                                                     self::$_memport);
                if ($isConnected === false) {
                    return null;
                }
            }
        } else {
            if(self::$_mem instanceof \Memcache) {
                self::$_mem->close();
                self::$_mem = null;
            }
        }
        return self::$_mem;
    }

    /**
     * 对于 GET 请求 , ngx_http_memc_purge_module 必须要求region/lang 这两个 cookie
     */
    public static function setDefaultCookie() 
    {
        if ($_SERVER['REQUEST_METHOD'] === "GET") {
            if (!isset($_COOKIE["region"])) {
                $_COOKIE["region"] = "cn";
                setcookie("region", $_COOKIE["region"], 0, '/');
            }

            if (!isset($_COOKIE["lang"])) {
                $_COOKIE["lang"] = "cn";
                setcookie("lang", $_COOKIE["lang"], 0, '/');
            }
        } 
    }

    /**
     * 和 nginx 规则相呼应的缓存写方法
     * @param string $page 整页的内容 , 以 compress 的形式放入 memcache
     * @return bool
     */
    public static function writePageToMem($page)
    {
        $mem = self::memInstance();
        $key = "http://" . $_SERVER['HTTP_HOST'] . $_SERVER['REQUEST_URI'];

        if (is_null($mem)) {
            return false;
        }

        if ($_SERVER['REQUEST_METHOD'] === "POST") {
            $key .= @file_get_contents("php://input");
        }
        
        // 对应 nginx.conf 配置项 ngx_memc_purge_memcpre, 默认值 : PLAYURL_
        $key = "PLAYURL_" . md5($key);

        if ($_SERVER['REQUEST_METHOD'] === "GET") {

            // 如果是 GET 请求 , 那么必须有 region 和 lang 两个 cookie
            if (isset($_COOKIE["region"])
                && isset($_COOKIE["lang"]))
            {
                $key .= $_COOKIE["region"] . $_COOKIE["lang"];
            } else {
                return false;
            }

        }

        $r = $mem->set($key, $page, MEMCACHE_COMPRESSED, 86400);

        // 这个 mem 连接是个单例模式,如果其他地方也要用,此处可不关闭
        self::memInstance("close");
        unset($mem);

        return $r;
    }
}
