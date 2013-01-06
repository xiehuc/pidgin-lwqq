pidgin-lwqq
===========

Intro
-----
 provide qq protocol for pidgin on linux. based on webqq service.  
 a pidgin plugin based on [lwqq](https://github.com/mathslinux/lwqq).
 a excellent safe useful library for webqq protocol.   

 some webqq proto detail comes from [qicq5](http://www.qicq5.com/) and [qqxieyi](http://bbs.qqxieyi.com/)

License: GPLv3


Quick Install
-------------

### Building From Source
    cmake .. 
    make
    sudo make install

### Building With Ubuntu-Online-Account Support
    cmake .. -DUOA=On
    make
    sudo make install

Build Option
------------

- UOA[=Off] 
> Ubuntu-Account-Online Support

- WITH_LIBEV[=On]
> enable libev .  
  put all http io waiting to libev thread.
  avoid block main ui thread. so would be smoothly.

- VERBOSE[=0]
> set the verbose level .0 means no verbose,3 means max verbose.

See [wiki](https://github.com/xiehuc/pidgin-lwqq/wiki) for the detail install guide.


### Notice

*recommand libcurl >=7.22.0*

Function list
-------------

### pidgin support

* send / recv buddy|group|discu messages
* send / recv picture messages
* send / recv qq face (you should use webqq faces theme in settings)
* avatar
* change buddy markname
* change buddy category
* confirm buddy added request
* visit buddy qzone
* group whisper message
* change status
* support multi webqq account
* support send/recv offline file
* support font style
* support block group message
* support recv file trans
* support local qqnumber cache
* support add friend

### empathy support (via telepathy-haze)

* send / recv text messages
* avatar
* **not** support local qqnumber cache


Known Issue
-----------

* send picture abnormal when libcurl < = 7.22.0
* telepathy-haze itself doesn't support group or picture.
* telepathy-haze store buddy list on /tmp/haze-XXX.
  every time startup recreate every thing.

简  介
-----
为linux的pidgin提供qq协议,基于webqq服务.
是在[lwqq](https://github.com/mathslinux/lwqq)基础上开发而来.
lwqq库是一个非常安全有效的webqq协议的库.  

快速安装
--------

### 从源代码编译

    cmake ..
    make
    sudo make install

### 编译 Ubuntu-Account-Online 支持

    cmake .. -DUOA=On
    make
    sudo make install

### 编译选项
- UOA[=Off] 
> Ubuntu-Account-Online Support

- WITH_LIBEV[=On]
> enable libev .  
  put all http io waiting to libev thread.
  avoid block main ui thread. so would be smoothly.

将全部的http io等待放到一个单独的线程中.
避免卡UI线程.能够提高程序的响应和体验.

    cmake .. -DWITH_LIBEV=On
    make
    sudo make install

功能列表
--------


### pidgin

* 支持发送接受 好友|群|讨论组 消息
* 支持发送接受图片
* 支持发送接受表情(在设置中使用webqq表情主题)
* 支持头像
* 支持设置好友备注
* 支持更改好友分组
* 支持确认添加好友请求
* 支持群的临时会话
* 支持访问QQ空间
* 支持更改在线状态
* 支持多账户登录
* 支持发送接受离线文件
* 支持文本样式
* 支持群消息屏蔽
* 支持接受文件传输
* 支持本地QQ号缓存机制
* 支持添加好友

### empathy support (via telepathy-haze)

* 支持发送接受文本消息
* 支持头像
* **不** 支持本地QQ号缓存机制

### core


已知问题
--------

* 当libcurl版本低于7.22.0时可能造成图片发送失败

*telepathy-haze 本身不支持群组聊天和图片显示.比较纠结.*

**注意:**
telepathy-haze 比较坑爹.居然不保存好友列表.!!
它只在/tmp/haze-XXXXXX 存放下文件.
所以速度非常受影响.
不知道为什么要这样设计.


