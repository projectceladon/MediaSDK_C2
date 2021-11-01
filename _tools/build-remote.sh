#!/bin/sh
# Sends updated source file to remote server and fires build remotely.
set -e

export PATH=/usr/bin

if [ -z "$remote_server" ]
then
    echo 'specify $remote_server like: user@server.com'
    exit 1
fi
if [ -z "$remote_dir" ]
then
    echo 'specify $remote_dir to android tree'
    exit 1
fi

echo "Build started $(date +%T)"

if [ ! -f 'lastsync~' ]
then
  touch --date "2008-01-01" 'lastsync~' # set old date to make all sources newer
fi

remote=${remote_server}:${remote_dir}/vendor/intel/mdp_msdk-c2-plugins/
find . -not -path '*/\.*' -type f -newer 'lastsync~' -exec scp {} $remote{} \;

touch 'lastsync~'
ssh ${remote_server} 'touch $HOME/build.signal'

echo "Update is sent $(date +%T)"

ssh ${remote_server} "while [ -f ~//build.signal ]; do sleep 1; done"

echo "Build finished $(date +%T)"

ssh ${remote_server} cat "~//build.log"
