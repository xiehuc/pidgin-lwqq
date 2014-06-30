pidgin-lwqq
===========

Intro
-----
 This plugin provide the QQ Protocol support for Pidgin on Linux. 
 It's based on the lwqq library [lwqq](https://github.com/mathslinux/lwqq) and use the WebQQ service.

License: GPLv3

ScreenShot
----------

### Linux ###

![gnome3](http://i.imgur.com/8kuEPHI.png)

If you want the effect above see [Gnome 3 support](https://github.com/xiehuc/pidgin-lwqq/wiki/gnome3-support)

### MacOSX ###

![adium](http://i.imgur.com/y4vweAL.png)

Quick Install
-------------

### Building From Source

Compile and install the [lwqq](https://github.com/xiehuc/lwqq) library

    mkdir build; cd build
    cmake ..
    make
    sudo make install

First use? See the [User Guide](https://github.com/xiehuc/pidgin-lwqq/wiki/simple-user-guide)

### Debian Package

To create a Debian package from the source::

   mkdir build; cd build
   cmake ..
   cpack


Build Option
------------

- VERBOSE[=0]
> set the verbose level from 0 (off) to 3 (maximum).

- SSL[=On]
> enable ssl support.

See [wiki](https://github.com/xiehuc/pidgin-lwqq/wiki) for a detailled install guide.


### Notice

*recommand libcurl >=7.22.0*

Function list
-------------

### Pidgin support

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

### Empathy not yet supported ###

Empathy is not yet supported because telepathy-haze can''t create the local cache 
which make lwqq performances really bad. 
So we do not recommand to use Empathy with lwqq anymore.

Known Issue
-----------

* send picture abnormal when libcurl < = 7.22.0
* telepathy-haze itself doesn't support group or picture.
* telepathy-haze store buddy list on /tmp/haze-XXX.
  every time startup recreate every thing.

