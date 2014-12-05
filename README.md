pidgin-lwqq
===========

![kuma企鹅](http://i.imgur.com/uxFYLIq.png)
![蚊香企鹅](http://i.imgur.com/NKpB4En.png)

简  介
-----
为linux的pidgin提供qq协议,基于webqq服务.
是在[lwqq](https://github.com/mathslinux/lwqq)基础上开发而来.
lwqq库是一个非常严谨有效的webqq协议的库.  
lwqq 即是 linux webqq 之意

#### 说明 ####

lwqq 基于[WebQQ](http://w.qq.com)，所以WebQQ做不到的，lwqq不可能做到。出了问题，建议用WebQQ尝试下，发issue前请先搜索下历史issue。lwqq 仅是 Pidgin 的一个插件，有些 Pidgin 不支持的，lwqq 也无法越权支持。

截  图
------

### linux ###

![pidgin](http://i.minus.com/ibxbiczTsJ0DFQ.png)

![gnome3](http://i.imgur.com/8kuEPHI.png)

想实现上面的效果? 猛击:[gnome3集成聊天](https://github.com/xiehuc/pidgin-lwqq/wiki/gnome3-support)

### mac osx ###

![adium](http://i.imgur.com/y4vweAL.png)

### windows ###

![win](http://ww2.sinaimg.cn/mw1024/70b249d6gw1e7m3pd9f0cj20lx0cktac.jpg)

快速安装
--------

### 从源代码编译

编译并安装 [lwqq](https://github.com/xiehuc/lwqq) 库

    mkdir build;cd build
    cmake ..
    make
    sudo make install

第一次使用? 猛击:[简易使用教程](https://github.com/xiehuc/pidgin-lwqq/wiki/simple-user-guide)

如果在安装lwqq的时候安装了python绑定, 则可以使用 `lwqq-cli.py --make-cache`
手动建立缓存. 好处是更加稳定. 如果在pidgin登录的时候好友数过多, 容易崩溃的可以试一试.

功能列表
--------

### pidgin

* 支持发送接受 好友|群|讨论组 消息
* 支持发送接受图片(webqq不稳定)
* 支持发送接受表情(在设置中使用webqq表情主题)
* 支持发送接受 输入提示|窗口摇动
* 支持设置 好友|群|讨论组 备注
* 完整支持讨论组
* 支持头像
* 支持更改好友分组
* 支持确认添加好友请求
* 支持群的临时会话
* 支持访问QQ空间
* 支持更改在线状态|群名片
* 支持多账户登录
* <del>支持发送接受离线文件</del>
* 支持文本样式
* 支持群消息屏蔽
* <del>支持接受文件传输(webqq已移除)</del>
* 支持本地QQ号缓存机制
* 支持添加好友|群
* <del>支持下载漫游记录(webqq不稳定)</del>

### empathy not support ###

由于telepathy-haze无法顺利的创建本地缓存，造成
性能低下，所以不推荐使用empathy配合lwqq。


已知问题
--------

* pidgin使用明文保存密码
    请配合使用[pidgin-gnome-keyring](https://code.google.com/p/pidgin-gnome-keyring/)并开启插件

* pidgin自身的问题,登录密码需要小于12位

* telepathy-haze 本身不支持群组聊天和图片显示.比较纠结.

**注意:**
telepathy-haze 比较坑爹.居然不保存好友列表.!!
它只在/tmp/haze-XXXXXX 存放下文件.
所以速度非常受影响.
不知道为什么要这样设计.

