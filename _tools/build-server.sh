# Performs building components when $HOME/build.signal file appears.
# Run the script from android tree directory.

if [ -z "$target_platform" ]
then
    echo 'specify $target_platform to be built'
    exit 1
fi

source build/envsetup.sh
lunch $target_platform

cd vendor/intel/mediasdk_git

echo 'include $(call all-subdir-makefiles)' > Android.mk
echo 'optional_subdirs = ["*"] ' > Android.bp

while true
do
    echo -n "."
    if [ -f "$HOME/build.signal" ]
    then
         echo -n "building...      "
         mm -j32 USE_MOCK_CODEC2=true BOARD_HAVE_MEDIASDK_SRC=true > ~/build.log
         rm $HOME/build.signal
         echo "done."
    else
        sleep 1;
    fi
done
