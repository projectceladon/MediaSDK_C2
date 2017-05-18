#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include "mfx_c2.h"
/*
 * Simple C++ Test Suite
 */

using namespace android;

static void prepareConfFile() {
#ifndef ANDROID
    std::string home = std::getenv("HOME");
#else
    std::string home = std::getenv("/data/local/tmp");
#endif
    std::ofstream fileConf(home + "/mfx_c2_store.conf");
    fileConf << "C2.Intel.sw_vd.h264 : libmfx_c2_components_sw.so" << std::endl;
    fileConf.close();
}

void test_GetC2ComponentStore() {

    prepareConfFile();

    std::cout << "preparedConfFile \n";

    std::shared_ptr<android::C2ComponentStore> componentStore;
    status_t status = GetC2ComponentStore(&componentStore);

    std::cout << "GetC2ComponentStore called \n";

    bool success = (status == C2_OK) && (componentStore != nullptr);
    if(!success) {
        std::cout << "%TEST_FAILED% time=0 testname=test_GetC2ComponentStore (c2_component_test) message=cannot create component store";
    }
    auto components = componentStore->getComponents();
    if(components.size() != 1) {
        std::cout << "%TEST_FAILED% time=0 testname=test_getComponentStore (c2_component_test) " <<
                "message=unexpected count of components == " << components.size();
    }
    if(components[0]->name != "C2.Intel.sw_vd.h264") {
        std::cout << "%TEST_FAILED% time=0 testname=test_getComponentStore (c2_component_test) message=unexpected component name";
    }
}

int main(int argc, char** argv) {

    (void)argc;
    (void)argv;

    std::cout << "%SUITE_STARTING% c2_component_test" << std::endl;
    std::cout << "%SUITE_STARTED%" << std::endl;

    std::cout << "%TEST_STARTED% test1 (c2_component_test)" << std::endl;
    test_GetC2ComponentStore();
    std::cout << "%TEST_FINISHED% time=0 test1 (c2_component_test)" << std::endl;

    std::cout << "%SUITE_FINISHED% time=0" << std::endl;

    return (EXIT_SUCCESS);
}

