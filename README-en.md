pidgin-lwqq
===========

Intro
-----
 provide qq protocol for pidgin on linux. based on webqq service.  
 a pidgin plugin based on [lwqq](https://github.com/mathslinux/lwqq).
 a excellent compact useful library for webqq protocol.   
 since the name may confused. lwqq is linux webqq

License: GPLv3

Effect
------

![gnome3](http://i.imgur.com/8kuEPHI.png)

want above effect? see:[gnome3 support](https://github.com/xiehuc/pidgin-lwqq/wiki/gnome3-support)

Quick Install
-------------

### Building From Source
    cmake .. 
    make
    sudo make install

first use? see:[user guide](https://github.com/xiehuc/pidgin-lwqq/wiki/simple-user-guide)

Build Option
------------

- VERBOSE[=0]
> set the verbose level .0 means no verbose,3 means max verbose.

- SSL[=On]
> enable ssl support.

See [wiki](https://github.com/xiehuc/pidgin-lwqq/wiki) for the detail install guide.


### Notice

*recommand libcurl >=7.22.0*

Function list
-------------

### pidgin support

* send / recv buddy|group|discu messages
* send / recv picture messages
* send / recv qq face (you should use webqq faces theme in settings)
* send / recv input notify | shake message
* change buddy|group|discu markname
* full support discu function
* avatar
* change buddy category
* confirm buddy added request
* visit buddy qzone
* group whisper message
* change status/business card
* support multi webqq account
* support send/recv offline file
* support font style
* support block group message
* support recv file trans
* support local qqnumber cache
* support add friend/group
* support import online chat log

### empathy not support ###

because telepathy-haze can not create local cache.
make lwqq performanced bad. 
so we do not recommand empathy with lwqq anymore.

Known Issue
-----------

* send picture abnormal when libcurl < = 7.22.0
* telepathy-haze itself doesn't support group or picture.
* telepathy-haze store buddy list on /tmp/haze-XXX.
  every time startup recreate every thing.

Donate
------

To support better develop (such as cross platform,other plugin compability,code refactoring),
You can donate this open source project [link](https://me.alipay.com/xiehuc)
