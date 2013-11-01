#!/bin/bash

#
cd download
if [ ! -d x264 ]; then
    git clone git://git.videolan.org/x264.git
fi

cd x264

git checkout master
git  pull origin +master:master
git checkout stable
git pull origin +stable:stable
if [ "$1" != "" ]; then
    git checkout $1
else # use curent default
    git checkout af8e768
fi

TARFILE=`./version.sh | grep X264_VERSION |awk '{print "x264-"$4"-"$5"-local.tar.gz"}' | sed 's/\"//g'`

cd ..
tar -hczf $TARFILE x264

echo "$TARFILE"
