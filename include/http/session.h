#ifndef WEBSERVER_SESSION_H_
#define WEBSERVER_SESSION_H_
#include <openssl/md5.h>
#include <ctime>
#include <string>
#include <map>
#include <list>

#include "locker.h"
using namespace std;

enum LOGIN_STATUS //用户状态
{
    LOGIN = 0,
    UNLOGIN,
    FIRST_LOGIN
};
struct User //用户信息
{
    string username;     //用户名
    string sessionid;    //sessionId
    LOGIN_STATUS status; //状态
    time_t last_time;    //上次请求时间
};
class Session
{
public:
    Session(int capacity, int timeout) : cap(capacity), timeout(timeout) //cap: Session最大容量 timeout:session保存时长（单位分钟）
    {
    }
    LOGIN_STATUS get_status(const string& sessionid) //返回用户状态
    {
        MutexLockGuard lock(mutex);
        if (cache.count(sessionid))
        {
            movetotop(sessionid);
            return cache[sessionid]->status;
        }
        return UNLOGIN;
    }
    string get_username(const string& sessionid) //返回用户名
    {
        MutexLockGuard lock(mutex);
        if (cache.count(sessionid))
        {
            movetotop(sessionid);
            return cache[sessionid]->username;
        }
        return "";
    }
    string put(const string& username) //首次添加返回sessionid
    {
        MutexLockGuard lock(mutex);
        string sessionid = MD5(username);
        if (0 == cache.count(sessionid))
        {
            if (users.size() >= cap)
            {
                cache.erase(users.back().sessionid);
                users.pop_back();
            }
            User user;
            user.username = username;
            user.sessionid = sessionid;
            user.last_time = time(nullptr);
            user.status = LOGIN;
            users.push_front(user);
            cache[sessionid] = users.begin();
        }
        cache[sessionid]->status = LOGIN;
        dealtimeout();
        return sessionid;
    }

private:
    void movetotop(const string& key) //将最新访问的用户移动到最前变
    {
        auto p = *cache[key];
        users.erase(cache[key]);
        users.push_front(p);
        cache[key] = users.begin();
        users.front().last_time = time(nullptr);
        dealtimeout();
    }
    void dealtimeout()
    {
        time_t curtime = time(nullptr);
        while (!users.empty() && users.back().last_time + timeout * 60 < curtime)
        {
            cache.erase(users.back().sessionid);
            users.pop_back();
        }
    }
     static string MD5(const string &src) //MD5 生成 sessionid
    {
        MD5_CTX ctx;

        string md5_string;
        unsigned char md[16] = {0};
        char tmp[40] = {0};
        string code = src + to_string(time(nullptr));
        MD5_Init(&ctx);
        MD5_Update(&ctx, code.c_str(), code.size());
        MD5_Final(md, &ctx);

        for (unsigned char i : md)
        {
            memset(tmp, 0x00, sizeof(tmp));
            sprintf(tmp, "%02X", i);
            md5_string += tmp;
        }
        return md5_string;
    }
    typedef list<User> List;
    size_t cap;                        //Cache容量
    int timeout;                       //session失效时间
    List users;                        //访问时间排序
    map<string, List::iterator> cache; //缓冲
    locker mutex;
};

#endif //WEBSERVER_SESSION_H_