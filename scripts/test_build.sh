#!/bin/sh

INSTALL_DIR=${HOME}/tmp/shdtool

cmake .. && make DESTDIR=${INSTALL_DIR} && make DESTDIR=${INSTALL_DIR} install
