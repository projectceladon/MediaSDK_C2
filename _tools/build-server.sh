# Performs building components when $HOME/build.signal file appears.
# Run the script from android tree directory.

if [ -z "$target_platform" ]
then
    echo 'specify $target_platform to be built'
    exit 1
fi

source build/envsetup.sh
lunch $target_platform

# If out/ dir was cleaned then we need to directly make targets depended from other dirs at least once.
# They cannot be built with mm from mdp_msdk-c2-plugins folder from scratch.
make -j32 BOARD_HAVE_MEDIASDK_SRC=true libmfx_c2_components_hw libmfx_c2_components_sw mfx_c2_store_unittests

cd vendor/intel/mdp_msdk-c2-plugins

while true
do
    echo -n "."
    if [ -f "$HOME/build.signal" ]
    then
         echo -n "building...      "
         # USE_MOCK_CODEC2=true/false could be specified to select mock/vndk build, default is vndk
         mm -j32 BOARD_HAVE_MEDIASDK_SRC=true > ~/build.log
         rm $HOME/build.signal
         echo "done."
    else
        sleep 1;
    fi
done
