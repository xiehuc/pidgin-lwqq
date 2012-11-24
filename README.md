pidgin-lwqq
===========

Intro
-----

a pidgin plugin based on lwqq, a excellent safe useful library for webqq protocol.   
see github:https://github.com/mathslinux/lwqq.

License: GPLv3


Build Option
------------

### Building From Source

    cmake ..
    make
    sudo make install

### Building With Ubuntu-Account-Online Support

    cmake .. -DUOA=On
    make
    sudo make install

### Notice

*recommand libcurl >=7.22.0*

Function list
-------------

### pidgin support

* send / recv text messages
* send / recv picture messages
* send / recv group messages
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

### empathy support (via telepathy-haze)

* send / recv text messages
* avatar

### core

* support local qqnumber cache

known issue
-----------

* send picture abnormal when libcurl < = 7.22.0
* use pidgin conversation intergrate gnome-shell plugin would cause crash.
enable "capible pidgin conversation intergrate" option in pidgin account setting to solve it.

*telepathy-haze itself doesn't support group or picture.*

**NOTE:**
telepathy-haze never store buddy list!!
It only saves file in /tmp/haze-XXXXXX
so never want to enjoy a good speed

简介
----

一个基于lwqq库的pidgin插件.  
lwqq库是一个非常安全有效的webqq协议的库.  
见github:https://github.com/mathslinux/lwqq  
*推荐libcurl 版本高于等于 7.22*

编译选项
--------

### 从源代码编译

    cmake ..
    make
    sudo make install

### 编译 Ubuntu-Account-Online 支持

    cmake .. -DUOA=On
    make
    sudo make install

功能列表
--------


### pidgin

* 支持发送接受文本消息
* 支持发送接受图片
* 支持群聊天
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

### empathy support (via telepathy-haze)

* 支持发送接受文本消息
* 支持头像

### core

* 提供本地QQ号缓存机制

已知问题
--------

* 当libcurl版本低于7.22.0时可能造成图片发送失败
* 使用gnome-shell的pidgin conversation intergrate 插件会导致段错误.
请在pidgin的账户设置中开启"兼容pidgin conversation intergrate"选项.

*telepathy-haze 本身不支持群组聊天和图片显示.比较纠结.*

**注意:**
telepathy-haze 比较坑爹.居然不保存好友列表.!!
它只在/tmp/haze-XXXXXX 存放下文件.
所以速度非常受影响.
不知道为什么要这样设计.


