#!/bin/bash

#
cd download
#if [ ! -d libav ]; then
#   git clone git://git.libav.org/libav.git
#fi

cd libav

git checkout master
git  pull origin
if [ "$1" != "" ]; then
    git checkout $1
else # use current default
    git checkout v10_beta2
fi

GITVERS=$(./version.sh)
SOURCE_DIR=$(echo "libav-$GITVERS""-local")
TARFILE=`echo "$SOURCE_DIR"".tar.bz2"`


cd ..

rm -rf "$SOURCE_DIR"
mkdir -p  "$SOURCE_DIR"
cp -Ppruvf libav/* "$SOURCE_DIR"
rm -rf  "$SOURCE_DIR"".git"

tar -hcjf $TARFILE $SOURCE_DIR

rm -rf $SOURCE_DIR
