# Downloads binaries from remote server, pulls them to android device
# and runs tests there.
# Tested on windows git-bash (msys) and ubuntu systems.
# Android device is assumed to be the only device connected to adb.
# Remote server should be set in $remote_server, could be localhost if run tests on build machine.
# android tree location on it - in $remote_dir, should be full path if run tests on build machine.
# If option -32 specified 32-bit output is tested, otherwise - 64-bit.
# Usages:
# _tools/run-tests.sh -32
# _tools/run-tests.sh
# Example of local run from android tree dir:
# remote_server=localhost remote_dir=$(pwd) target_platform=gordon_peak ./run-tests.sh

set -e -u # -u to avoid wrong execution of command like "rm -rf ${var}/*"

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
if [ -z "$target_platform" ]
then
    echo 'specify $target_platform to run tests'
    exit 1
fi

bitness=64
remote_lib=lib64

if [ "$#" -ge 1 ]
then
    if [ "$1" == '-32' ]
    then
        bitness=32
        remote_lib=lib
    fi
fi

remote_output=$remote_dir/out/target/product/$target_platform/

tests_folder=c2-msdk-tests

local_dir=/tmp/$tests_folder

device_dir=/data/local/tmp/$tests_folder

mkdir -p $local_dir
rm -rf $local_dir/*
mkdir $local_dir/service

libs=\
libmfx_mock_c2_components.so,\
libmfx_c2_components_hw.so,\
libmfxhw${bitness}.so

execs=\
mfx_c2_store_unittests,\
mfx_c2_components_unittests,\
mfx_c2_mock_unittests

scp $remote_server:${remote_output}vendor/\{$remote_lib/\{$libs\},bin/\{$execs\}$bitness\} ${local_dir}

service=hardware.intel.media.c2@1.0-service

service_system_libs="\
libstagefright_codec2_vndk_mfx.so,\
libstagefright_bufferpool_mfx@1.0.so,\
hardware.google.media.c2@1.0.so,\
libcodec2_hidl_utils@1.0.so,\
libstagefright_bufferpool@1.0.so,\
libstagefright_codec2.so,\
libstagefright_codec2_vndk.so"

if ssh $remote_server "test -e ${remote_output}vendor/bin/hw/${service}"
then
    scp $remote_server:${remote_output}vendor/bin/hw/${service} ${local_dir}/service

    scp $remote_server:${remote_output}system/lib/\{$service_system_libs\} ${local_dir}/service

    system_exec=mfx_c2_service_unittests
    scp $remote_server:$remote_output/system/bin/${system_exec}${bitness} ${local_dir}
fi

system_libs="libcodec2_hidl_client.so,$service_system_libs"

scp $remote_server:${remote_output}system/$remote_lib/\{$system_libs\} ${local_dir}

if adb shell "[ ! -w /system ]"
then
    echo "/system is inaccessible, remounting..."
    adb root
    adb wait-for-device
    adb remount
    adb wait-for-device
fi

adb shell "rm -rf ${device_dir}/*"

cd /tmp # do cd and adb separately to use msys mangling for /tmp path
MSYS_NO_PATHCONV=1 adb push ${tests_folder}/. ${device_dir} # and not use mangling for $device_dir
# adb push src folder ends with /. to work same way on windows and ubuntu adb versions

adb wait-for-device

gtest_filter=${gtest_filter:-'*'}

adb shell 'cd '${device_dir}'; \
for exec_name in $(find . ! -name \*.so -type f); do chmod a+x $exec_name; done;\
status=0; \
for exec_name in ./*unittests*; do \
LD_LIBRARY_PATH=.:/system/'$remote_lib'/vndk-28 ./$exec_name --gtest_filter='$gtest_filter' --dump-output; \
if [ $? -ne 0 ]; then status=1; fi; \
done; \
exit $status'
