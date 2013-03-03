#!/bin/sh

gdb pidgin -ex "handle SIGPIPE nostop pass" -ex "r"
