#!/usr/bin/env bash

SOURCE_DIR=`dirname "$(readlink -f "$0")"`
mkdir -p $SOURCE_DIR/libs/sources;

git clone git@github.com:UPPAALModelChecker/UUtils.git $SOURCE_DIR/libs/sources/UUtils;
mkdir -p $SOURCE_DIR/libs/sources/UUtils/build
cd $SOURCE_DIR/libs/sources/UUtils/build
cmake -DCMAKE_INSTALL_PREFIX=$SOURCE_DIR/libs/UUtils ..
make -j $(nproc) install
export UUtils_ROOT=$SOURCE_DIR/libs/UUtils
cd $SOURCE_DIR

git clone git@github.com:UPPAALModelChecker/UDBM2.git $SOURCE_DIR/libs/sources/UDBM;
mkdir -p $SOURCE_DIR/libs/sources/UDBM/build
cd $SOURCE_DIR/libs/sources/UDBM/build
cmake -DCMAKE_INSTALL_PREFIX=$SOURCE_DIR/libs/UDBM ..
make -j $(nproc) install
export UDBM_ROOT=$SOURCE_DIR/libs/UDBM
cd $SOURCE_DIR

