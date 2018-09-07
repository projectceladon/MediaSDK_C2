/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include <gtest/gtest.h>
#include "mfx_frame_constructor.h"
#include "streams/h264/aud_mw_e.264.h"
#include "streams/h265/AMVP_A_MTK_4.bit.h"
#include "streams/vp9/stream_nv12_176x144_cqp_g30_100.vp9.ivf.h"

const size_t READ_ALL = std::numeric_limits<size_t>::max();
const size_t DO_NOT_READ = 0;

#define PARAMS_DESCRIBED(...) { __VA_ARGS__, #__VA_ARGS__ }

struct TestStream
{
    MfxC2FrameConstructorType type;
    const StreamDescription& stream_desc;
    std::string description;
};

TestStream test_streams[] = {
    PARAMS_DESCRIBED(MfxC2FC_AVC, aud_mw_e_264),
    PARAMS_DESCRIBED(MfxC2FC_HEVC, AMVP_A_MTK_4_bit),
    PARAMS_DESCRIBED(MfxC2FC_VP9, stream_nv12_176x144_cqp_g30_100_vp9_ivf)
};

// Returns bool result if Reader is able to read some data from input stream.
static bool PassThrough(
    std::shared_ptr<IMfxC2FrameConstructor> frame_constructor, StreamReader& reader,
    const StreamReader::Slicing& slicing,
    const StreamDescription& stream,
    bool complete_frame, mfxU64 pts,
    size_t read_size, std::string* output)
{
    StreamDescription::Region region {};
    bool header = false;

    bool res = reader.Read(slicing, &region, &header);

    if (res) {

        mfxStatus sts = frame_constructor->Load(
            (const mfxU8*)&stream.data.front() + region.offset,
            region.size, pts, header, complete_frame);
        EXPECT_EQ(sts, MFX_ERR_NONE);

        std::shared_ptr<mfxBitstream> bitstream = frame_constructor->GetMfxBitstream();
        EXPECT_NE(bitstream, nullptr);

        if(bitstream != nullptr) {
            EXPECT_EQ(bitstream->TimeStamp, pts)
                << bitstream->TimeStamp << " instead of " << pts;
            bool mfx_complete_frame = (bitstream->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME) != 0;
            EXPECT_EQ(mfx_complete_frame, complete_frame);

            if(read_size > bitstream->DataLength) {
                read_size = bitstream->DataLength;
            }

            if(nullptr != output) {
                *output = std::string((const char*)bitstream->Data + bitstream->DataOffset, read_size);
                bitstream->DataOffset += read_size;
                bitstream->DataLength -= read_size;
            }
        }

        sts = frame_constructor->Unload();
        EXPECT_EQ(sts, MFX_ERR_NONE);

        EXPECT_FALSE(frame_constructor->WasEosReached());
    }
    return res;
}

// Pass stream with chunks, expects to get the same byte sequence.
// When input bitstream is split by NAL Unuts it tests complete_frame
// flag handling as well.
TEST(FrameConstructor, PassEverything)
{
    struct TestCase
    {
        MfxC2FrameConstructorType type;
        const StreamDescription& stream_desc;
        StreamReader::Slicing load_slicing;
        size_t read_size;
        // Flag value to be passed into Load method.
        // It doesn't correspond real frames, test checks if output
        // mfxBitstream::DataFlag is modifired accordingly.
        bool complete_frame;
        std::string description;
    };

    TestCase g_test_cases[] = {
        PARAMS_DESCRIBED(MfxC2FC_AVC, aud_mw_e_264, StreamReader::Slicing::NalUnit(), READ_ALL, false),
        PARAMS_DESCRIBED(MfxC2FC_AVC, aud_mw_e_264, StreamReader::Slicing::NalUnit(), READ_ALL, true),
        PARAMS_DESCRIBED(MfxC2FC_AVC, aud_mw_e_264, StreamReader::Slicing(1000), READ_ALL, false),
        PARAMS_DESCRIBED(MfxC2FC_AVC, aud_mw_e_264, StreamReader::Slicing(1000), 960, false),
        PARAMS_DESCRIBED(MfxC2FC_HEVC, AMVP_A_MTK_4_bit, StreamReader::Slicing::NalUnit(), READ_ALL, false),
        PARAMS_DESCRIBED(MfxC2FC_HEVC, AMVP_A_MTK_4_bit, StreamReader::Slicing::NalUnit(), READ_ALL, true),
        PARAMS_DESCRIBED(MfxC2FC_HEVC, AMVP_A_MTK_4_bit, StreamReader::Slicing(1000), READ_ALL, false),
        PARAMS_DESCRIBED(MfxC2FC_HEVC, AMVP_A_MTK_4_bit, StreamReader::Slicing(1000), 960, false),
        PARAMS_DESCRIBED(MfxC2FC_VP9, stream_nv12_176x144_cqp_g30_100_vp9_ivf, StreamReader::Slicing::Frame(), READ_ALL, false),
        PARAMS_DESCRIBED(MfxC2FC_VP9, stream_nv12_176x144_cqp_g30_100_vp9_ivf, StreamReader::Slicing::Frame(), READ_ALL, true),
        PARAMS_DESCRIBED(MfxC2FC_VP9, stream_nv12_176x144_cqp_g30_100_vp9_ivf, StreamReader::Slicing(1000), READ_ALL, false),
        PARAMS_DESCRIBED(MfxC2FC_VP9, stream_nv12_176x144_cqp_g30_100_vp9_ivf, StreamReader::Slicing(1000), 900, false),
    };

    for (const TestCase& test_case : g_test_cases) {

        SCOPED_TRACE(testing::Message() << "Test case: " << test_case.description);

        std::shared_ptr<IMfxC2FrameConstructor> frame_constructor =
            MfxC2FrameConstructorFactory::CreateFrameConstructor(test_case.type);
        EXPECT_NE(frame_constructor, nullptr);
        if (nullptr == frame_constructor) continue;

        mfxStatus sts = frame_constructor->Init(0, {}/*init parameters don't matter*/);
        EXPECT_EQ(sts, MFX_ERR_NONE);

        SingleStreamReader reader(&test_case.stream_desc);

        mfxU64 pts = 0;
        mfxU64 loaded_pts = 0;
        mfxU32 read_count = 0;

        std::ostringstream oss;
        std::string output;

        while(PassThrough(frame_constructor, reader,
            StreamReader::Slicing(test_case.load_slicing), test_case.stream_desc,
            test_case.complete_frame, pts,
            test_case.read_size, &output)) {

            loaded_pts = pts;
            pts += 33333;
            read_count++;

            oss << output;
        }

        frame_constructor->SetEosMode(true);

        std::shared_ptr<mfxBitstream> bitstream = frame_constructor->GetMfxBitstream();

        size_t data_left = 0;

        if (bitstream != nullptr) { // read leftover

            mfxU64 expected_pts = (test_case.read_size == READ_ALL) ? 0 : loaded_pts;

            EXPECT_EQ(bitstream->TimeStamp, expected_pts) << bitstream->TimeStamp << " instead of " << pts;

            data_left = bitstream->DataLength;

            oss.write((const char*)bitstream->Data + bitstream->DataOffset, bitstream->DataLength);
            bitstream->DataOffset += bitstream->DataLength;
            bitstream->DataLength = 0;
        }

        // expects some data leftover iff not read out all data from mfxBitstream
        EXPECT_EQ(data_left != 0, test_case.read_size != READ_ALL);

        EXPECT_TRUE(frame_constructor->WasEosReached());

        frame_constructor->Close();

        if (test_case.load_slicing.GetType() == StreamReader::Slicing::Type::Frame &&
            test_case.stream_desc.fourcc == MFX_CODEC_VP9) { // for VP9 will skipped IVF header

            EXPECT_EQ(oss.str().size() + read_count * 12 /* size of ivf frame header*/ + 32 /* size of ivf stream header */, test_case.stream_desc.data.size());

        } else {

            EXPECT_EQ(oss.str().size(), test_case.stream_desc.data.size());

            EXPECT_EQ(oss.str(),
                std::string(&test_case.stream_desc.data.front(), test_case.stream_desc.data.size()));
        }
    }
}

static size_t GetHeaderEndPos(const StreamDescription& stream)
{
    return (stream.fourcc == MFX_CODEC_VP9) ? 32/*size of IVF Header*/ : std::max<>(stream.sps.offset + stream.sps.size,
        stream.pps.offset + stream.pps.size);
}

// Reads some part of a stream from beginning, resets,
// reads the stream from different position.
// Depending of inclusuion the header in parts read before and after reset,
// original header should or should not be added to output mfxBitstream.
TEST(FrameConstructor, Reset)
{
    for(const auto& test_stream : test_streams) {

        SCOPED_TRACE(testing::Message() << "Stream: " << test_stream.description);

        const StreamDescription& stream = test_stream.stream_desc;

        struct TestCase
        {
            size_t read_before_reset;
            size_t seek_after_reset;
            size_t read_after_reset;
            bool header_expected;
            std::string description;
        };

        size_t header_end_pos = GetHeaderEndPos(stream);

        TestCase test_cases[] = {
            { header_end_pos, header_end_pos + 100, 100, true,
                "Read header, reset, skip header, bitstream should be appended with sps and pps" },
            { header_end_pos + 100, header_end_pos + 200, 100, true,
                "Read header + more data, reset, skip header, bitstream should be appended with sps and pps" },
            { 1, header_end_pos + 100, 100, false,
                "Read incomplete header, reset, skip header, bitstream matches input" },
            { header_end_pos, 0, 100, false,
                "Read header, reset, read header, bitstream matches input" },
        };

        for(const auto& test_case : test_cases) {

            SCOPED_TRACE(testing::Message() << "Case: " << test_case.description);

            std::shared_ptr<IMfxC2FrameConstructor> frame_constructor =
                MfxC2FrameConstructorFactory::CreateFrameConstructor(test_stream.type);
            EXPECT_NE(frame_constructor, nullptr);
            if (nullptr == frame_constructor) continue;

            mfxStatus sts = frame_constructor->Init(0, {}/*init parameters don't matter*/);
            EXPECT_EQ(sts, MFX_ERR_NONE);

            SingleStreamReader reader(&stream);

            bool res = PassThrough(frame_constructor, reader,
                StreamReader::Slicing(test_case.read_before_reset), stream,
                false/*complete_frame*/, 0/*pts*/,
                DO_NOT_READ, nullptr/*output*/);
            ASSERT_TRUE(res);

            sts = frame_constructor->Reset();
            EXPECT_EQ(sts, MFX_ERR_NONE);

            res = reader.Seek(test_case.seek_after_reset);
            ASSERT_TRUE(res);

            std::string actual_data;

            res = PassThrough(frame_constructor, reader,
                StreamReader::Slicing(test_case.read_after_reset), stream,
                false/*complete_frame*/, 0/*pts*/,
                READ_ALL, &actual_data);
            ASSERT_TRUE(res);

            std::string expected_data { &stream.data.front() + test_case.seek_after_reset,
                test_case.read_after_reset };

            if(test_case.header_expected) {
                EXPECT_EQ(actual_data.size(),
                    test_case.read_after_reset + stream.sps.size + stream.pps.size);
                std::string sps { &stream.data.front() + stream.sps.offset, stream.sps.size };
                std::string pps { &stream.data.front() + stream.pps.offset, stream.pps.size };

                EXPECT_EQ(actual_data, sps + pps + expected_data);
            } else {
                EXPECT_EQ(actual_data.size(), test_case.read_after_reset);

                EXPECT_EQ(actual_data, expected_data);
            }

            frame_constructor->Close();
        }
    }
}

static std::shared_ptr<mfxBitstream> MakeBitstream(const char* contents)
{
    size_t len = strlen(contents);

    std::shared_ptr<mfxBitstream> res = std::make_shared<mfxBitstream>();
    MFX_ZERO_MEMORY((*res));
    res->MaxLength = len;
    res->DataLength = len;
    res->Data = (mfxU8*)contents;

    return res;
}

// Tests FrameConstructor::SaveHeaders with a sequence:
// 1) SaveHeaders(sps1, pps1, false)
// 2) Load first 100 bytes from input
// 3) Read data from mfxBitstream (all data or leave some)
// 4) SaveHeaders(sps2, pps2, true)
// 5) Seek input to pos 200
// 6) Load next 100 bytes from input
// 7) Read all data from mfxBitStream
// 8) Regardless of option on step 3) expect read data is:
//       sps2 + pps2 + read on step 6).
TEST(FrameConstructor, SaveHeaders)
{
    for(const auto& test_stream : test_streams) {

        SCOPED_TRACE(testing::Message() << "Stream: " << test_stream.description);

        const StreamDescription& stream = test_stream.stream_desc;

        if (stream.fourcc == MFX_CODEC_VP9) { // VP9 hasn't SPS, PPS
            break;
        }

        for(bool read_all : { true, false }) {

            SCOPED_TRACE(testing::Message() << "Case: " <<
                (read_all ? "Read whole mfxBitstream" : "Leave some data in mfxBitstream"));

            size_t feed_size = 100;
            size_t read_size = read_all ? READ_ALL : 90;
            size_t seek_pos = 200;

            std::shared_ptr<IMfxC2FrameConstructor> frame_constructor =
                MfxC2FrameConstructorFactory::CreateFrameConstructor(test_stream.type);
            EXPECT_NE(frame_constructor, nullptr);
            if (nullptr == frame_constructor) continue;

            mfxStatus sts = frame_constructor->Init(0, {}/*init parameters don't matter*/);
            EXPECT_EQ(sts, MFX_ERR_NONE);

            SingleStreamReader reader(&stream);

            std::shared_ptr<mfxBitstream> sps1 = MakeBitstream("sps1");
            std::shared_ptr<mfxBitstream> pps1 = MakeBitstream("pps1");

            sts = frame_constructor->SaveHeaders(sps1, pps1, false);
            EXPECT_EQ(sts, MFX_ERR_NONE);

            std::string output;
            bool res = PassThrough(frame_constructor, reader,
                StreamReader::Slicing(feed_size), stream,
                false/*complete_frame*/, 0/*pts*/,
                read_size, &output);
            ASSERT_TRUE(res);
            std::shared_ptr<mfxBitstream> sps2 = MakeBitstream("sps2");
            std::shared_ptr<mfxBitstream> pps2 = MakeBitstream("pps2");

            sts = frame_constructor->SaveHeaders(sps2, pps2, true);
            EXPECT_EQ(sts, MFX_ERR_NONE);

            res = reader.Seek(seek_pos);
            ASSERT_TRUE(res);

            res = PassThrough(frame_constructor, reader,
                StreamReader::Slicing(feed_size), stream,
                false/*complete_frame*/, 0/*pts*/,
                READ_ALL, &output);
            ASSERT_TRUE(res);

            EXPECT_EQ(output,
                std::string("sps2pps2") + std::string(&stream.data.front() + seek_pos, feed_size))
                    << output.size();

            frame_constructor->Close();
        }
    }
}
