# libopus 1.4 as an ESP-IDF component.
#
# The tree under this directory is an unmodified subset of the upstream release
# (include/, celt/, silk/, src/), with the x86/ARM/MIPS intrinsics, the test
# programs and the demo tools removed - none of them build for RISC-V. Nothing
# in the sources is patched, so a newer libopus can be dropped in by repeating
# that trimming and refreshing the file lists below.
#
# Built fixed-point: the ESP32-P4 has only a single-precision FPU, and SILK's
# float path leans on doubles. The Opus float *API* stays enabled, which is
# what src/analysis.c and src/mlp*.c are for (bandwidth and bitrate decisions).

set(CELT_SOURCES
    "celt/bands.c"
    "celt/celt.c"
    "celt/celt_decoder.c"
    "celt/celt_encoder.c"
    "celt/celt_lpc.c"
    "celt/cwrs.c"
    "celt/entcode.c"
    "celt/entdec.c"
    "celt/entenc.c"
    "celt/kiss_fft.c"
    "celt/laplace.c"
    "celt/mathops.c"
    "celt/mdct.c"
    "celt/modes.c"
    "celt/pitch.c"
    "celt/quant_bands.c"
    "celt/rate.c"
    "celt/vq.c")

set(SILK_SOURCES
    "silk/A2NLSF.c"
    "silk/CNG.c"
    "silk/HP_variable_cutoff.c"
    "silk/LPC_analysis_filter.c"
    "silk/LPC_fit.c"
    "silk/LPC_inv_pred_gain.c"
    "silk/LP_variable_cutoff.c"
    "silk/NLSF2A.c"
    "silk/NLSF_VQ.c"
    "silk/NLSF_VQ_weights_laroia.c"
    "silk/NLSF_decode.c"
    "silk/NLSF_del_dec_quant.c"
    "silk/NLSF_encode.c"
    "silk/NLSF_stabilize.c"
    "silk/NLSF_unpack.c"
    "silk/NSQ.c"
    "silk/NSQ_del_dec.c"
    "silk/PLC.c"
    "silk/VAD.c"
    "silk/VQ_WMat_EC.c"
    "silk/ana_filt_bank_1.c"
    "silk/biquad_alt.c"
    "silk/bwexpander.c"
    "silk/bwexpander_32.c"
    "silk/check_control_input.c"
    "silk/code_signs.c"
    "silk/control_SNR.c"
    "silk/control_audio_bandwidth.c"
    "silk/control_codec.c"
    "silk/debug.c"
    "silk/dec_API.c"
    "silk/decode_core.c"
    "silk/decode_frame.c"
    "silk/decode_indices.c"
    "silk/decode_parameters.c"
    "silk/decode_pitch.c"
    "silk/decode_pulses.c"
    "silk/decoder_set_fs.c"
    "silk/enc_API.c"
    "silk/encode_indices.c"
    "silk/encode_pulses.c"
    "silk/gain_quant.c"
    "silk/init_decoder.c"
    "silk/init_encoder.c"
    "silk/inner_prod_aligned.c"
    "silk/interpolate.c"
    "silk/lin2log.c"
    "silk/log2lin.c"
    "silk/pitch_est_tables.c"
    "silk/process_NLSFs.c"
    "silk/quant_LTP_gains.c"
    "silk/resampler.c"
    "silk/resampler_down2.c"
    "silk/resampler_down2_3.c"
    "silk/resampler_private_AR2.c"
    "silk/resampler_private_IIR_FIR.c"
    "silk/resampler_private_down_FIR.c"
    "silk/resampler_private_up2_HQ.c"
    "silk/resampler_rom.c"
    "silk/shell_coder.c"
    "silk/sigm_Q15.c"
    "silk/sort.c"
    "silk/stereo_LR_to_MS.c"
    "silk/stereo_MS_to_LR.c"
    "silk/stereo_decode_pred.c"
    "silk/stereo_encode_pred.c"
    "silk/stereo_find_predictor.c"
    "silk/stereo_quant_pred.c"
    "silk/sum_sqr_shift.c"
    "silk/table_LSF_cos.c"
    "silk/tables_LTP.c"
    "silk/tables_NLSF_CB_NB_MB.c"
    "silk/tables_NLSF_CB_WB.c"
    "silk/tables_gain.c"
    "silk/tables_other.c"
    "silk/tables_pitch_lag.c"
    "silk/tables_pulses_per_block.c")

set(SILK_FIXED_SOURCES
    "silk/fixed/LTP_analysis_filter_FIX.c"
    "silk/fixed/LTP_scale_ctrl_FIX.c"
    "silk/fixed/apply_sine_window_FIX.c"
    "silk/fixed/autocorr_FIX.c"
    "silk/fixed/burg_modified_FIX.c"
    "silk/fixed/corrMatrix_FIX.c"
    "silk/fixed/encode_frame_FIX.c"
    "silk/fixed/find_LPC_FIX.c"
    "silk/fixed/find_LTP_FIX.c"
    "silk/fixed/find_pitch_lags_FIX.c"
    "silk/fixed/find_pred_coefs_FIX.c"
    "silk/fixed/k2a_FIX.c"
    "silk/fixed/k2a_Q16_FIX.c"
    "silk/fixed/noise_shape_analysis_FIX.c"
    "silk/fixed/pitch_analysis_core_FIX.c"
    "silk/fixed/process_gains_FIX.c"
    "silk/fixed/regularize_correlations_FIX.c"
    "silk/fixed/residual_energy16_FIX.c"
    "silk/fixed/residual_energy_FIX.c"
    "silk/fixed/schur64_FIX.c"
    "silk/fixed/schur_FIX.c"
    "silk/fixed/vector_ops_FIX.c"
    "silk/fixed/warped_autocorrelation_FIX.c")

# Kept for reference; only built if FIXED_POINT is turned off below.
set(SILK_FLOAT_SOURCES
    "silk/float/LPC_analysis_filter_FLP.c"
    "silk/float/LPC_inv_pred_gain_FLP.c"
    "silk/float/LTP_analysis_filter_FLP.c"
    "silk/float/LTP_scale_ctrl_FLP.c"
    "silk/float/apply_sine_window_FLP.c"
    "silk/float/autocorrelation_FLP.c"
    "silk/float/burg_modified_FLP.c"
    "silk/float/bwexpander_FLP.c"
    "silk/float/corrMatrix_FLP.c"
    "silk/float/encode_frame_FLP.c"
    "silk/float/energy_FLP.c"
    "silk/float/find_LPC_FLP.c"
    "silk/float/find_LTP_FLP.c"
    "silk/float/find_pitch_lags_FLP.c"
    "silk/float/find_pred_coefs_FLP.c"
    "silk/float/inner_product_FLP.c"
    "silk/float/k2a_FLP.c"
    "silk/float/noise_shape_analysis_FLP.c"
    "silk/float/pitch_analysis_core_FLP.c"
    "silk/float/process_gains_FLP.c"
    "silk/float/regularize_correlations_FLP.c"
    "silk/float/residual_energy_FLP.c"
    "silk/float/scale_copy_vector_FLP.c"
    "silk/float/scale_vector_FLP.c"
    "silk/float/schur_FLP.c"
    "silk/float/sort_FLP.c"
    "silk/float/warped_autocorrelation_FLP.c"
    "silk/float/wrappers_FLP.c")

set(OPUS_SOURCES
    "src/analysis.c"
    "src/mapping_matrix.c"
    "src/mlp.c"
    "src/mlp_data.c"
    "src/opus.c"
    "src/opus_decoder.c"
    "src/opus_encoder.c"
    "src/opus_multistream.c"
    "src/opus_multistream_decoder.c"
    "src/opus_multistream_encoder.c"
    "src/opus_projection_decoder.c"
    "src/opus_projection_encoder.c"
    "src/repacketizer.c")

idf_component_register(
    SRCS ${CELT_SOURCES} ${SILK_SOURCES} ${SILK_FIXED_SOURCES} ${OPUS_SOURCES}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "." "celt" "silk" "silk/fixed" "src")

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    OPUS_BUILD
    FIXED_POINT=1
    VAR_ARRAYS
    HAVE_LRINT
    HAVE_LRINTF
    PACKAGE_VERSION=\"1.4\")

# Upstream is not warning-clean under ESP-IDF's flags, and it is vendored code:
# don't let its warnings bury ours.
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unused-but-set-variable
    -Wno-unused-variable
    -Wno-maybe-uninitialized
    -Wno-sign-compare
    # silk/main.h declares the NSQ callbacks over a fixed-size array the
    # callers index more loosely; upstream builds with this warning off too.
    -Wno-stringop-overread)
