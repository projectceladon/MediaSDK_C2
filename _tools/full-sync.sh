# Sends all mdp_msdk-c2-plugins folder structure to remote server.
# Assumes that it is run from the mdp_msdk-c2-plugins root folder.
# Copies local sources to the remote server.
# Anything existed on the remote server will be deleted (backup saved as c2_prev.backup.N.tar.gz).
# Usage: remote_server=user@host.intel.com remote_dir=android-O ./_tools/full-sync.sh
set -e

if [ -z "$remote_server" ]
then
    echo 'specify $remote_server like: user@host.intel.com'
    exit 1
fi
if [ -z "$remote_dir" ]
then
    echo 'specify $remote_dir to android tree'
    exit 1
fi

tar --exclude=.git* --exclude=*~ -czf c2.tar.gz *
scp c2.tar.gz ${remote_server}:${remote_dir}/vendor/intel/
rm c2.tar.gz

ssh ${remote_server} "
    set -e
    cd $remote_dir/vendor/intel/
    tar -czf c2_prev.backup.$(date +%s).tar.gz mdp_msdk-c2-plugins/
    rm -rf mdp_msdk-c2-plugins/*
    tar xzmf c2.tar.gz -C mdp_msdk-c2-plugins/
    rm c2.tar.gz"
