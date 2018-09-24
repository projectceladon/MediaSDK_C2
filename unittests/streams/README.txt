Original stream: "Time Lapse of Bay" (IMG_5040_CLIPCHAMP_keep.mp4)
    Source: https://www.videvo.net/video/time-lapse-of-bay/4658/
    Author: Omar Ramo (http://www.video.net/profile/omarramo/)
    Licence: Creative Commons 3.0 Unported (CC BY 3.0)
    Resolition 1920x1080
    Format mp4 RAW NV12
    Aspect Ratio 16.9
    Frame Rate: 30 fps
    Duration 34 sec

This does not imply any endorsement of Intel(R) or of Intel's software products.

Original stream was decoded by ffmpeg, resized by sample_vpp (uses MSDK VPP) and were added numbers of frames by ffmpeg for creation 2 RAW sequences:
    stream_nv12_176x144_100.yuv - NV12 176x144
    stream_nv12_352x288_100.yuv - NV12 352x288

All test streams self coded from these RAW video sequences by sample_encode or ffmpeg

NAME                                                 CODEC         RESOLUTION    FOURCC    GOP SIZE    FRAMES NUMBER
stream_nv12_176x144_cqp_g30_100.264                  h264          176x144       NV12      30          100
stream_nv12_352x288_cqp_g15_100.264                  h264          352x288       NV12      15          100
stream_nv12_176x144_cqp_g30_100.265                  h265          176x144       NV12      30          100
stream_nv12_352x288_cqp_g15_100.265                  h265          352x288       NV12      15          100
stream_nv12_176x144_cqp_g30_100.vp9.ivf              VP9           176x144       NV12      30          100
stream_nv12_352x288_cqp_g15_100.vp9.ivf              VP9           352x288       NV12      15          100

More details of encoded streams in the per components READMEs.
