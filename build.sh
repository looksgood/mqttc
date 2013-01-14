#!/usr/bin/env bash

aclocal
automake --add-missing --foreign --copy
autoconf

./configure && make
