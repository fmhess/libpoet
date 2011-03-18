#!/bin/bash

set -o errexit

TOP_DIR=$(pwd)
TOP_DIST_DIR=$TOP_DIR/test/libpoet-$(date +%F)

cd test
cvs export -rHEAD libpoet
mv libpoet $TOP_DIST_DIR
cd $TOP_DIST_DIR/doc
cp -a $TOP_DIR/doc/style .
cp -a $TOP_DIR/doc/javascript .
cd boostbook
make
cd $TOP_DIST_DIR/..
tar -czf $TOP_DIST_DIR.tgz $(basename $TOP_DIST_DIR) --owner=0 --group=0
