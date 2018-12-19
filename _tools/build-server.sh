# Performs building components when $HOME/build.signal file appears.
# Run the script from android tree directory.

if [ -z "$target_platform" ]
then
    echo 'specify $target_platform to be built'
    exit 1
fi

components="libmfx_c2_components_hw mfx_c2_store_unittests"

case $mediasdk_source in
    open)
        make_options="BOARD_HAVE_MEDIASDK_OPEN_SOURCE=true"
        ;;
    closed)
        make_options="BOARD_HAVE_MEDIASDK_SRC=true"
        components="$components libmfx_c2_components_sw"
        ;;
    *)
        echo 'specify $mediasdk_source as "open" or "closed"'
        exit 1
esac

source build/envsetup.sh
lunch $target_platform

# If out/ dir was cleaned then we need to directly make targets depended from other dirs at least once.
# They cannot be built with mm from mdp_msdk-c2-plugins folder from scratch.
make -j32 $make_options $components
cd vendor/intel/mdp_msdk-c2-plugins

while true
do
    echo -n "."
    if [ -f "$HOME/build.signal" ]
    then
         echo -n "building...      "
         mm -j32 $make_options > ~/build.log
         rm $HOME/build.signal
         echo "done."
    else
        sleep 1;
    fi
done
