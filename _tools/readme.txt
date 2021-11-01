bin_to_cpp.py
-------------
Converts all files from source folder to their c array representations.
Output .h files and .cpp files are written to <dst_dir>.
Usage: python _tools/bin_to_c.py -i ../streams -o unittests/streams

build-remote.sh
---------------
Sends updated source file to remote server and fires build remotely.

build-server.sh
---------------
Performs build when $HOME/build.signal file appears.

count-lines.sh
--------------
Computes c++ code lines count recursively from the current dir

full-sync.sh
------------
Sends all mdp_msdk-c2-plugins folder structure to remote server.
Assumes that it is run from the mdp_msdk-c2-plugins root folder.
Copies local sources to the remote server.
Anything existed on the remote server will be deleted (backup saved as _prev.backup.N.tar.gz).
Usage: remote_server=user@host.intel.com remote_dir=android-O ./_tools/full-sync.sh

gather_tests.py
---------------
Produces html report file with description tables of all gtest tests
found in specified folder.
Usage: python _tools/gather_tests.py -i unittests/src -o tests.html

gerrit_stat.py
--------------
Returns usage statistics for the given Gerrit repository
Usage: python gerrit_stat.py -s server -p port -r repository

run-tests.sh
------------
Downloads binaries from remote server, pulls them to android device
and runs tests there.
Android device is assumed to be the only device connected to adb.
Remote server should be set in $remote_server,
android tree location on it - in $remote_dir.
If option -32 specified 32-bit output is tested, otherwise - 64-bit.
Usages:
_tools/run-tests.sh -32
_tools/run-tests.sh
