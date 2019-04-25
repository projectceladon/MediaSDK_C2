/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/
#include "c2_store_test.h"

#include <thread>
#include <fstream>
#include <gtest/gtest.h>
#include <codec2/hidl/client.h>

#define SERVICE_NAME "hardware.intel.media.c2@1.0-service"
#define VINTF_NAME "hardware.google.media.c2"

// C2 service needs some changes in root filesystem to be accessed with hwbinder.
// Vendor interface for IComponentStore should be enabled.
// For that purpose IComponentStore entry is created in manifest.xml and compatibility_matrix.xml
// in /vendor/etc/vintf/ dir.
// This change is a permission grant, should not affect system behaviour,
// so no backup/restore actions are provided.

// To emulate service hardware.intel.media.c2@1.0-service is run as
// regular executable in a child process.
// It is stopped upon tests completion, no matter successful or not.
// If mfx_c2_service_unittests32 (64) crashes - child process will be alive
// and that prevents adb shell console from close.
// In that case open another adb shell console and run there command from
// StopC2Service function (see below).

class C2Client : public testing::Environment
{
private:
    static void StopC2Service()
    {
        std::system("ps -e | "
            "awk '$NF == \"" SERVICE_NAME "\" { print $2 }' | "
            "xargs -n 1 kill -INT 1>/dev/null 2>&1");
    }

    static void StartC2Service()
    {
        int result = std::system("LD_LIBRARY_PATH=./service:/system/lib/vndk-28 "
            "./service/" SERVICE_NAME " &");
        ASSERT_EQ(result, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    static void WriteHalEntry(bool full, std::fstream& file)
    {
        file <<
        "    <hal format=\"hidl\">\n"
        "        <name>hardware.google.media.c2</name>\n";
        if (full) file <<
        "        <transport>hwbinder</transport>\n";
        file <<
        "        <version>1.0</version>\n"
        "        <interface>\n"
        "            <name>IComponentStore</name>\n"
        "            <instance>default</instance>\n"
        "            <instance>software</instance>\n"
        "        </interface>\n";
        if (full) file << "        <fqname>@1.0::IComponentStore/default</fqname>\n";
        file << "    </hal>\n";
    }

    // This function enables vintf for IComponentStore.
    static void EnableVendorInterface()
    {
        const std::string vintf_dir{"/vendor/etc/vintf/"};

        const char* manifest_name{"manifest"};
        const char* compatibility_matrix_name{"compatibility_matrix"};

        bool update_done = false;
        for (const char* file_name : {manifest_name, compatibility_matrix_name}) {

            std::fstream file(vintf_dir + file_name + ".xml");
            ASSERT_TRUE(file.good());

            std::string end_root = std::string("</") + std::string(file_name) + ">";
            // root item might slightly differ from file name
            std::replace(end_root.begin(), end_root.end(), '_', '-');

            while (!file.eof()) {

                std::ifstream::pos_type insert_pos = file.tellg();

                std::string line;
                std::getline(file, line);
                if (line.find(VINTF_NAME) != std::string::npos) {
                    break; // found our service, no need to update
                }

                size_t offset = line.find(end_root);
                if (offset != std::string::npos) {
                    insert_pos += offset;

                    file.seekp(insert_pos);
                    WriteHalEntry(file_name == manifest_name, file);
                    file << end_root << std::endl;
                    update_done = true;
                    break;
                }
            }
        }

        if (update_done) {
            std::system("stop hwservicemanager; start hwservicemanager"); // actualize changes
            std::this_thread::sleep_for(std::chrono::seconds(1));
            // otherwise DecodeBitExact hangs next run
            std::system("stop vendor.gralloc-2-0; start vendor.gralloc-2-0");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void SetUp() override
    {
        char env_update[]{"LD_LIBRARY_PATH="}; // variable to provide char* argument to putenv
        putenv(env_update); // reset LD_LIBRARY_PATH env to be not inherited by child processed run below

        StopC2Service();
        PrepareConfFile();
        EnableVendorInterface();
        StartC2Service();
    }

    void TearDown() override
    {
        RestoreConfFile();
        StopC2Service();
    }

    static C2Client* g_client;
};

C2Client* C2Client::g_client = [] {
    C2Client* client = new C2Client();
    ::testing::AddGlobalTestEnvironment(client);
    return client;
} ();

std::shared_ptr<android::Codec2Client> GetCodec2Client() {
    return android::Codec2Client::CreateFromService("default", false);
}

TEST(MfxC2Service, Start)
{
    std::shared_ptr<android::Codec2Client> client = GetCodec2Client();
    EXPECT_TRUE(client);
}

TEST(MfxC2Service, getComponents)
{
    std::shared_ptr<android::Codec2Client> client = GetCodec2Client();
    ASSERT_NE(client, nullptr);

    auto actual_components = client->listComponents();
    EXPECT_EQ(actual_components.size(), std::extent<decltype(g_components)>::value);

    for (const auto& actual_component : actual_components) {
        std::string actual_name = actual_component.name;

        auto found = std::find_if(std::begin(g_components), std::end(g_components),
            [=] (const auto& item) { return actual_name == item.component_name; } );

        EXPECT_NE(found, std::end(g_components));
    }
}

TEST(MfxC2Service, createComponent)
{
    std::shared_ptr<android::Codec2Client> client = GetCodec2Client();
    ASSERT_NE(client, nullptr);

    for(const auto& component_desc : g_components) {
        std::shared_ptr<android::Codec2Client::Component> component;
        c2_status_t status =
            client->createComponent(component_desc.component_name, nullptr, &component);
        EXPECT_EQ(status, component_desc.creation_status);
        if(component_desc.creation_status == C2_OK) {
            EXPECT_NE(component, nullptr);

            if(component != nullptr) {

                EXPECT_EQ(component->getName(), component_desc.component_name);
                component = nullptr;
            }
       }
    }
}

// Tests if all components from the list could be created via Codec2Client::createInterface.
// Also test checks that component returns valid information via interface (b.e., returns name).
TEST(MfxC2Service, createInterface)
{
    std::shared_ptr<android::Codec2Client> client = GetCodec2Client();
    ASSERT_NE(client, nullptr);

    for(const auto& component_desc : g_components) {
        std::shared_ptr<android::Codec2Client::Interface> component_itf;
        c2_status_t status = client->createInterface(component_desc.component_name, &component_itf);
        EXPECT_EQ(status, component_desc.creation_status);

        if(component_desc.creation_status == C2_OK) {
            EXPECT_NE(component_itf, nullptr);

            if(component_itf != nullptr) {
                EXPECT_EQ(component_itf->getName(), component_desc.component_name);

                component_itf = nullptr;
            }
        }
    }
}
