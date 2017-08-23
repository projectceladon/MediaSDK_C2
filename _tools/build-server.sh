# Performs build when $HOME/build.signal file appears.
android_tree=~/android-O
build_target=r2_bxt_rvp-userdebug
components="\
    libmfx_c2_store \
    libmfx_mock_c2_components \
    libmfx_c2_components_pure \
    libmfx_c2_components_sw \
    libmfx_c2_components_hw \
    mfx_c2_components_unittests \
    mfx_c2_store_unittests \
    mfx_c2_mock_unittests"

cd $android_tree
source build/envsetup.sh
lunch $build_target

while true
do
    echo -n "."
    if [ -f "$HOME/build.signal" ]
    then
         echo -n "building...      "
         make -j32 BOARD_HAVE_MEDIASDK_SRC=true $components > ~/build.log
         rm $HOME/build.signal
         echo "done."
    else
        sleep 1;
    fi
done
