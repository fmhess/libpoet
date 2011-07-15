#!/bin/bash

set -o errexit

TOP_DIR=$(pwd)
TOP_DIST_DIR=$TOP_DIR/test/libpoet-$(date +%F)

cd test
svn export -rHEAD https://svn.boost.org/svn/boost/sandbox/libpoet/trunk libpoet
mv libpoet $TOP_DIST_DIR
cd $TOP_DIST_DIR/doc
rsync -a --exclude ".svn" $TOP_DIR/doc/style .
rsync -a --exclude ".svn" $TOP_DIR/doc/javascript .
cd boostbook
make
cd $TOP_DIST_DIR/..
tar -czf $TOP_DIST_DIR.tgz $(basename $TOP_DIST_DIR) --owner=0 --group=0
