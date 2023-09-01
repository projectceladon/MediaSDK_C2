# Fuzzing with Mediasdk_c2

## How to run it

1. Build the fuzzer with `make mfx_c2_avc_dec_fuzzer`.
2. Find the files in out/target/product/caas/data/fuzz and move mfx_c2_avc_dec_fuzzer to the previous level(`/data/fuzz/x86_64/mfx_c2_avc_dec_fuzzer`).
3. adb push data/fuzz to your device.
4. Execute the fuzzer with '/data/fuzz/x86_64/mfx_c2_avc_dec_fuzzer'.

For more information, see the [libfuzzer](https://source.android.com/docs/security/test/libfuzzer#run-your-fuzzer)