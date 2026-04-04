#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#include <process.h>
#else
#include <unistd.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define EXE(X) X ".exe"
#else
#define EXE(X) X
#endif

#ifdef __clang__
#  define CC "clang"
#elif _MSC_VER
#  define CC "cl"
#else
#  define CC "cc"
#endif

static void usage() {
  fprintf(stderr, "just call 'build' without arguments\n");
}

static int run(char ** args) {
  assert(args && args[0]);

#ifdef _WIN32
  if (0 == _spawnvp(_P_WAIT, args[0], (const char * const *)args)) {
    return 0;
  }
#else
  pid_t pid = fork();
  if (pid == 0) {
    execvp(args[0], args);
    abort();
  } else if (pid > 0) {
    int sl = 0;
    assert(0 <= waitpid(pid, &sl, 0));
    if (WIFEXITED(sl)) return WEXITSTATUS(sl);
  }
#endif

  fprintf(stderr, "failed to run child process: %s\n", args[0]);
  return 1;
}

static const char * files[] = {
  "libopus/src/opus_decoder.c",
  "libopus/celt/bands.c",
  "libopus/celt/celt.c",
  "libopus/celt/cwrs.c",
  "libopus/celt/entcode.c",
  "libopus/celt/entdec.c",
  "libopus/celt/entenc.c",
  "libopus/celt/kiss_fft.c",
  "libopus/celt/laplace.c",
  "libopus/celt/mathops.c",
  "libopus/celt/mdct.c",
  "libopus/celt/modes.c",
  "libopus/celt/pitch.c",
  "libopus/celt/celt_lpc.c",
  "libopus/celt/quant_bands.c",
  "libopus/celt/rate.c",
  "libopus/celt/vq.c",
  "libopus/silk/CNG.c",
  "libopus/silk/code_signs.c",
  "libopus/silk/init_decoder.c",
  "libopus/silk/decode_core.c",
  "libopus/silk/decode_frame.c",
  "libopus/silk/decode_parameters.c",
  "libopus/silk/decode_indices.c",
  "libopus/silk/decode_pulses.c",
  "libopus/silk/decoder_set_fs.c",
  "libopus/silk/dec_API.c",
  "libopus/silk/enc_API.c",
  "libopus/silk/encode_indices.c",
  "libopus/silk/encode_pulses.c",
  "libopus/silk/gain_quant.c",
  "libopus/silk/interpolate.c",
  "libopus/silk/LP_variable_cutoff.c",
  "libopus/silk/NLSF_decode.c",
  "libopus/silk/NSQ.c",
  "libopus/silk/NSQ_del_dec.c",
  "libopus/silk/PLC.c",
  "libopus/silk/shell_coder.c",
  "libopus/silk/tables_gain.c",
  "libopus/silk/tables_LTP.c",
  "libopus/silk/tables_NLSF_CB_NB_MB.c",
  "libopus/silk/tables_NLSF_CB_WB.c",
  "libopus/silk/tables_other.c",
  "libopus/silk/tables_pitch_lag.c",
  "libopus/silk/tables_pulses_per_block.c",
  "libopus/silk/VAD.c",
  "libopus/silk/control_audio_bandwidth.c",
  "libopus/silk/quant_LTP_gains.c",
  "libopus/silk/VQ_WMat_EC.c",
  "libopus/silk/HP_variable_cutoff.c",
  "libopus/silk/NLSF_encode.c",
  "libopus/silk/NLSF_VQ.c",
  "libopus/silk/NLSF_unpack.c",
  "libopus/silk/NLSF_del_dec_quant.c",
  "libopus/silk/process_NLSFs.c",
  "libopus/silk/stereo_LR_to_MS.c",
  "libopus/silk/stereo_MS_to_LR.c",
  "libopus/silk/check_control_input.c",
  "libopus/silk/control_SNR.c",
  "libopus/silk/init_encoder.c",
  "libopus/silk/control_codec.c",
  "libopus/silk/A2NLSF.c",
  "libopus/silk/ana_filt_bank_1.c",
  "libopus/silk/biquad_alt.c",
  "libopus/silk/bwexpander_32.c",
  "libopus/silk/bwexpander.c",
  "libopus/silk/debug.c",
  "libopus/silk/decode_pitch.c",
  "libopus/silk/inner_prod_aligned.c",
  "libopus/silk/lin2log.c",
  "libopus/silk/log2lin.c",
  "libopus/silk/LPC_analysis_filter.c",
  "libopus/silk/LPC_inv_pred_gain.c",
  "libopus/silk/table_LSF_cos.c",
  "libopus/silk/NLSF2A.c",
  "libopus/silk/NLSF_stabilize.c",
  "libopus/silk/NLSF_VQ_weights_laroia.c",
  "libopus/silk/pitch_est_tables.c",
  "libopus/silk/resampler.c",
  "libopus/silk/resampler_down2_3.c",
  "libopus/silk/resampler_down2.c",
  "libopus/silk/resampler_private_AR2.c",
  "libopus/silk/resampler_private_down_FIR.c",
  "libopus/silk/resampler_private_IIR_FIR.c",
  "libopus/silk/resampler_private_up2_HQ.c",
  "libopus/silk/resampler_rom.c",
  "libopus/silk/sigm_Q15.c",
  "libopus/silk/sort.c",
  "libopus/silk/sum_sqr_shift.c",
  "libopus/silk/stereo_decode_pred.c",
  "libopus/silk/stereo_encode_pred.c",
  "libopus/silk/stereo_find_predictor.c",
  "libopus/silk/stereo_quant_pred.c",
  "libopus/silk/float/apply_sine_window_FLP.c",
  "libopus/silk/float/corrMatrix_FLP.c",
  "libopus/silk/float/encode_frame_FLP.c",
  "libopus/silk/float/find_LPC_FLP.c",
  "libopus/silk/float/find_LTP_FLP.c",
  "libopus/silk/float/find_pitch_lags_FLP.c",
  "libopus/silk/float/find_pred_coefs_FLP.c",
  "libopus/silk/float/LPC_analysis_filter_FLP.c",
  "libopus/silk/float/LTP_analysis_filter_FLP.c",
  "libopus/silk/float/LTP_scale_ctrl_FLP.c",
  "libopus/silk/float/noise_shape_analysis_FLP.c",
  "libopus/silk/float/prefilter_FLP.c",
  "libopus/silk/float/process_gains_FLP.c",
  "libopus/silk/float/regularize_correlations_FLP.c",
  "libopus/silk/float/residual_energy_FLP.c",
  "libopus/silk/float/solve_LS_FLP.c",
  "libopus/silk/float/warped_autocorrelation_FLP.c",
  "libopus/silk/float/wrappers_FLP.c",
  "libopus/silk/float/autocorrelation_FLP.c",
  "libopus/silk/float/burg_modified_FLP.c",
  "libopus/silk/float/bwexpander_FLP.c",
  "libopus/silk/float/energy_FLP.c",
  "libopus/silk/float/inner_product_FLP.c",
  "libopus/silk/float/k2a_FLP.c",
  "libopus/silk/float/levinsondurbin_FLP.c",
  "libopus/silk/float/LPC_inv_pred_gain_FLP.c",
  "libopus/silk/float/pitch_analysis_core_FLP.c",
  "libopus/silk/float/scale_copy_vector_FLP.c",
  "libopus/silk/float/scale_vector_FLP.c",
  "libopus/silk/float/schur_FLP.c",
  "libopus/silk/float/sort_FLP.c",
  0
};
static const char * base_args[] = {
  EXE(CC), "-g", "-Wall", "-Ilibopus/include", "mkv2wav.c", "-o", EXE("mkv2wav")
};

int opus_cc(char * file) {
  char * obj = malloc(strlen(file + 3));
  sprintf(obj, "%s.o", file);

  // Quick hack to avoid compiling twice
  FILE * tst = fopen(obj, "rb");
  if (tst) {
    fclose(tst);
    return 0;
  }

  fprintf(stderr, "Compiling %s\n", file);

  char * args[] = {
    EXE(CC), "-g", "-c", "-Wno-everything",
    "-Ilibopus/include", "-Ilibopus/celt", "-Ilibopus/silk", "-Ilibopus/silk/float",
    "-DUSE_ALLOCA", "-Drestrict=", "-DOPUS_BUILD",
    file, "-o", obj, 0 };
  return run(args);
}

int main(int argc, char ** argv) {
  if (argc != 1) return (usage(), 1);

  char ** args = malloc(sizeof(base_args) + sizeof(files));
  char ** arg = args;
  for (const char ** ptr = base_args; *ptr; ptr++, arg++) *arg = strdup(*ptr);

  for (const char ** ptr = files; *ptr; ptr++, arg++) {
    if (opus_cc(strdup(*ptr))) return 1;
    *arg = malloc(strlen(*ptr) + 3);
    sprintf(*arg, "%s.o", *ptr);
  }

  fprintf(stderr, "Compiling mkv2wav\n");
  for (char ** ptr = args; *ptr; ptr++) puts(*ptr);
  return run(args);
}
