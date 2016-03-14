/*
 *  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdlib.h>
#include <new>

#include "third_party/googletest/src/include/gtest/gtest.h"
#include "test/acm_random.h"
#include "test/util.h"
#include "./vpx_config.h"
#include "vpx_ports/msvc.h"

#undef CONFIG_COEFFICIENT_RANGE_CHECKING
#define CONFIG_COEFFICIENT_RANGE_CHECKING 1
#include "vp10/encoder/dct.c"

using libvpx_test::ACMRandom;

namespace {
void reference_dct_1d(const double *in, double *out, int size) {
  const double PI = 3.141592653589793238462643383279502884;
  const double kInvSqrt2 = 0.707106781186547524400844362104;
  for (int k = 0; k < size; ++k) {
    out[k] = 0;
    for (int n = 0; n < size; ++n) {
      out[k] += in[n] * cos(PI * (2 * n + 1) * k / (2 * size));
    }
    if (k == 0)
      out[k] = out[k] * kInvSqrt2;
  }
}

typedef void (*FdctFuncRef)(const double *in, double *out, int size);
typedef void (*IdctFuncRef)(const double *in, double *out, int size);
typedef void (*FdctFunc)(const tran_low_t *in, tran_low_t *out);
typedef void (*IdctFunc)(const tran_low_t *in, tran_low_t *out);

class TransTestBase {
 public:
  virtual ~TransTestBase() {}

 protected:
  void RunFwdAccuracyCheck() {
    tran_low_t *input  = new tran_low_t[txfm_size_];
    tran_low_t *output = new tran_low_t[txfm_size_];
    double *ref_input  = new double[txfm_size_];
    double *ref_output = new double[txfm_size_];

    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int count_test_block = 5000;
    for (int ti =  0; ti < count_test_block; ++ti) {
      for (int ni = 0; ni < txfm_size_; ++ni) {
        input[ni] = rnd.Rand8() - rnd.Rand8();
        ref_input[ni] = static_cast<double>(input[ni]);
      }

      fwd_txfm_(input, output);
      fwd_txfm_ref_(ref_input, ref_output, txfm_size_);

      for (int ni = 0; ni < txfm_size_; ++ni) {
        EXPECT_LE(
            abs(output[ni] - static_cast<tran_low_t>(round(ref_output[ni]))),
            max_error_);
      }
    }

    delete[] input;
    delete[] output;
    delete[] ref_input;
    delete[] ref_output;
  }

  double max_error_;
  int txfm_size_;
  FdctFunc fwd_txfm_;
  FdctFuncRef fwd_txfm_ref_;
};

typedef std::tr1::tuple<FdctFunc, FdctFuncRef, int, int> FdctParam;
class Vp10FwdTxfm
    : public TransTestBase,
      public ::testing::TestWithParam<FdctParam> {
 public:
  virtual void SetUp() {
    fwd_txfm_ = GET_PARAM(0);
    fwd_txfm_ref_ = GET_PARAM(1);
    txfm_size_ = GET_PARAM(2);
    max_error_ = GET_PARAM(3);
  }
  virtual void TearDown() {}
};

TEST_P(Vp10FwdTxfm, RunFwdAccuracyCheck) {
  RunFwdAccuracyCheck();
}

INSTANTIATE_TEST_CASE_P(
    C, Vp10FwdTxfm,
    ::testing::Values(
        FdctParam(&fdct4, &reference_dct_1d, 4, 1),
        FdctParam(&fdct8, &reference_dct_1d, 8, 1),
        FdctParam(&fdct16, &reference_dct_1d, 16, 2)));
}  // namespace
