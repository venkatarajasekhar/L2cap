#!/bin/bash

touch AUTHORS ChangeLog NEWS README || exit 1
aclocal || exit 1
autoconf || exit 1
automake --add-missing || exit 1
