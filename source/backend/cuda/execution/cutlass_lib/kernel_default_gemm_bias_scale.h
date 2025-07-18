/***************************************************************************************************
 * Copyright (c) 2017 - 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **************************************************************************************************/

/*! \file
    \brief 
      Default kernel-level GEMM definitions combine threadblock-scoped matrix multiply-add with
      the appropriate threadblock-scoped epilogue.
  
      Note, CUTLASS epilogues universally target row-major outputs. Column-major outputs are
      accommodated by exchanging A and B operands and assuming transposed layouts. Partial
      specializations here choose 'device::GemmTransposed' to implement this functionality.
*/

#pragma once

#include "cutlass/cutlass.h"

#include "cutlass/layout/matrix.h"
#include "cutlass/numeric_types.h"
#include "cutlass/arch/wmma.h"

#include "epilogue_bias_scale.h"
#include "cutlass/epilogue/thread/linear_combination.h"

#include "cutlass/gemm/gemm.h"
#include "cutlass/gemm/kernel/gemm.h"
#include "cutlass/gemm/kernel/gemm_pipelined.h"
#include "cutlass/gemm/threadblock/default_mma_core_sm75.h"
#include "cutlass/gemm/threadblock/default_mma_core_sm70.h"
#include "cutlass/gemm/threadblock/default_mma_core_sm80.h"
#include "cutlass/gemm/threadblock/default_mma.h"
#include "cutlass/gemm/threadblock/default_mma_core_simt.h"
#include "cutlass/gemm/threadblock/threadblock_swizzle.h"

#include "default_epilogue_tensor_op_bias_scale.h"
#include "cutlass/epilogue/threadblock/default_epilogue_volta_tensor_op.h"
#include "cutlass/epilogue/threadblock/default_epilogue_simt.h"
#include "cutlass/transform/threadblock/predicated_tile_iterator.h"

#if defined(CUTLASS_ARCH_WMMA_ENABLED)
#include "cutlass/epilogue/threadblock/default_epilogue_wmma_tensor_op.h"
#endif //CUTLASS_ARCH_WMMA_ENABLED

////////////////////////////////////////////////////////////////////////////////

namespace cutlass {
namespace gemm {
namespace kernel {

////////////////////////////////////////////////////////////////////////////////

template <
    /// Element type for A matrix operand
    typename ElementA_,
    /// Layout type for A matrix operand
    typename LayoutA_,
    /// Access granularity of A matrix in units of elements
    int kAlignmentA,
    /// Element type for B matrix operand
    typename ElementB_,
    /// Layout type for B matrix operand
    typename LayoutB_,
    /// Access granularity of B matrix in units of elements
    int kAlignmentB,
    /// Element type for C matrix operands
    typename ElementC_,
    /// Layout type for C and D matrix operands
    typename LayoutC_,
    /// Element type for internal accumulation
    typename ElementAccumulator,
    /// Element type for D matrix operands -- bias
    typename ElementD_,
    /// Element type for E matrix operands -- scale
    typename ElementE_,
    /// Operator class tag
    typename OperatorClass,
    /// Tag indicating architecture to tune for
    typename ArchTag,
    /// Threadblock-level tile size (concept: GemmShape)
    typename ThreadblockShape,
    /// Warp-level tile size (concept: GemmShape)
    typename WarpShape,
    /// Warp-level tile size (concept: GemmShape)
    typename InstructionShape,
    /// Epilogue output operator
    typename EpilogueOutputOp,
    /// Threadblock-level swizzling operator
    typename ThreadblockSwizzle,
    /// Number of stages used in the pipelined mainloop
    int Stages,
    /// If true, kernel is configured to support serial reduction in the
    /// epilogue
    bool SplitKSerial,
    /// Operation performed by GEMM
    typename Operator,
    /// Use zfill or predicate for out-of-bound cp.async
    SharedMemoryClearOption SharedMemoryClear = SharedMemoryClearOption::kNone,
    /// Gather operand A by using an index array
    bool GatherA = false,
    /// Gather operand B by using an index array
    bool GatherB = false,
    /// Scatter result D by using an index array
    bool ScatterD = false,
    ///
    typename Enable = void
>
struct DefaultGemmBiasScale;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/// Partial specialization for Ampere Architecture
template <
    /// Element type for A matrix operand
    typename ElementA,
    /// Layout type for A matrix operand
    typename LayoutA,
    /// Access granularity of A matrix in units of elements
    int kAlignmentA,
    /// Element type for B matrix operand
    typename ElementB,
    /// Layout type for B matrix operand
    typename LayoutB,
    /// Access granularity of A matrix in units of elements
    int kAlignmentB,
    /// Element type for C matrix operands
    typename ElementC,
    /// Layout type for C and D matrix operand
    typename LayoutC,
    /// Element type for internal accumulation
    typename ElementAccumulator,
    /// Element type for D matrix operands -- bias
    typename ElementD,
    /// Element type for E matrix operands -- scale
    typename ElementE,
    /// Threadblock-level tile size (concept: GemmShape)
    typename ThreadblockShape,
    /// Warp-level tile size (concept: GemmShape)
    typename WarpShape,
    /// Warp-level tile size (concept: GemmShape)
    typename InstructionShape,
    /// Epilogue output operator
    typename EpilogueOutputOp,
    /// Threadblock-level swizzling operator
    typename ThreadblockSwizzle,
    /// Number of stages used in the pipelined mainloop
    int Stages,
    /// If true, kernel is configured to support serial reduction in the
    /// epilogue
    bool SplitKSerial,
    /// Operation performed by GEMM
    typename Operator,
    /// Use zfill or predicate for out-of-bound cp.async
    SharedMemoryClearOption SharedMemoryClear,
    /// Gather operand A by using an index array
    bool GatherA,
    /// Gather operand B by using an index array
    bool GatherB,
    /// Scatter result D by using an index array
    bool ScatterD
>
struct DefaultGemmBiasScale<ElementA, LayoutA, kAlignmentA, ElementB, LayoutB, kAlignmentB, ElementC,
                   LayoutC, ElementAccumulator, ElementD, ElementE, arch::OpClassTensorOp,
                   arch::Sm80, ThreadblockShape, WarpShape, InstructionShape,
                   EpilogueOutputOp,
                   ThreadblockSwizzle, Stages, SplitKSerial,
                   Operator, SharedMemoryClear, GatherA, GatherB, ScatterD> {

  static_assert(platform::is_same<LayoutC, layout::RowMajor>::value
             || platform::is_same<LayoutC, layout::AffineRankN<2>>::value,
             "Epilogue in the kernel level must be row major");

  /// Define the threadblock-scoped matrix multiply-accumulate
  using Mma = typename cutlass::gemm::threadblock::DefaultMma<
      ElementA, LayoutA, kAlignmentA, ElementB, LayoutB, kAlignmentB,
      ElementAccumulator, LayoutC, arch::OpClassTensorOp, arch::Sm80,
      ThreadblockShape, WarpShape, InstructionShape, Stages,
      Operator, false, SharedMemoryClear, GatherA, GatherB>::ThreadblockMma;

  static const int kPartitionsK = ThreadblockShape::kK / WarpShape::kK;

  /// Define the epilogue
  using RegularEpilogue =
      typename cutlass::epilogue::threadblock::DefaultEpilogueTensorOpBiasScale<
          ThreadblockShape, typename Mma::Operator, kPartitionsK, EpilogueOutputOp,
          ElementD, ElementE,
          EpilogueOutputOp::kCount, ScatterD>::Epilogue;

  using Affine2Epilogue =
      typename cutlass::epilogue::threadblock::DefaultEpilogueTensorOpBiasScaleAffineRankN<
          2, ThreadblockShape, typename Mma::Operator, kPartitionsK, EpilogueOutputOp,
          ElementD, ElementE,
          EpilogueOutputOp::kCount>::Epilogue;

  using Epilogue = typename cutlass::platform::conditional<cutlass::platform::is_same<LayoutC, layout::RowMajor>::value,
                                                  RegularEpilogue,
                                                  Affine2Epilogue>::type;

  /// Define the kernel-level GEMM operator.
  using GemmKernel = kernel::GemmBiasScale<Mma, Epilogue, ThreadblockSwizzle, SplitKSerial>;
};

////////////////////////////////////////////////////////////////////////////////

/// Partial specialization for Turing Architecture
template <
  /// Element type for A matrix operand
  typename ElementA,
  /// Layout type for A matrix operand
  typename LayoutA,
  /// Access granularity of A matrix in units of elements
  int kAlignmentA,
  /// Element type for B matrix operand
  typename ElementB,
  /// Layout type for B matrix operand
  typename LayoutB,
  /// Access granularity of B matrix in units of elements
  int kAlignmentB,
  /// Element type for C matrix operands
  typename ElementC,
  /// Element type for internal accumulation
  typename ElementAccumulator,
  /// Element type for D matrix operands -- bias
  typename ElementD,
  /// Element type for E matrix operands -- scale
  typename ElementE,
  /// Threadblock-level tile size (concept: GemmShape)
  typename ThreadblockShape,
  /// Warp-level tile size (concept: GemmShape)
  typename WarpShape,
  /// Warp-level tile size (concept: GemmShape)
  typename InstructionShape,
  /// Epilogue output operator
  typename EpilogueOutputOp,
  /// Threadblock-level swizzling operator
  typename ThreadblockSwizzle,
  /// If true, kernel is configured to support serial reduction in the epilogue
  bool SplitKSerial,
  /// Operation performed by GEMM
  typename Operator,
  /// Use zfill or predicate for out-of-bound cp.async
  SharedMemoryClearOption SharedMemoryClear,
  /// Gather operand A by using an index array
  bool GatherA,
  /// Gather operand B by using an index array
  bool GatherB,
  /// Scatter result D by using an index array
  bool ScatterD
>
struct DefaultGemmBiasScale<
  ElementA, LayoutA, kAlignmentA,
  ElementB, LayoutB, kAlignmentB,
  ElementC, layout::RowMajor,
  ElementAccumulator,
  ElementD,
  ElementE,
  arch::OpClassTensorOp,
  arch::Sm75,
  ThreadblockShape,
  WarpShape,
  InstructionShape,
  EpilogueOutputOp,
  ThreadblockSwizzle,
  2,
  SplitKSerial,
  Operator,
  SharedMemoryClear,
  GatherA,
  GatherB,
  ScatterD
> {

  /// Define the threadblock-scoped matrix multiply-accumulate
  using Mma = typename cutlass::gemm::threadblock::DefaultMma<
    ElementA,
    LayoutA,
    kAlignmentA,
    ElementB,
    LayoutB,
    kAlignmentB,
    ElementAccumulator,
    layout::RowMajor,
    arch::OpClassTensorOp,
    arch::Sm75,
    ThreadblockShape,
    WarpShape,
    InstructionShape,
    2,
    Operator,
    false,
    SharedMemoryClear,
    GatherA,
    GatherB
  >::ThreadblockMma;

  static const int kPartitionsK = ThreadblockShape::kK / WarpShape::kK;

  /// Define the epilogue
  using Epilogue = typename cutlass::epilogue::threadblock::DefaultEpilogueTensorOpBiasScale<
    ThreadblockShape,
    typename Mma::Operator,
    kPartitionsK,
    EpilogueOutputOp,
    ElementD, ElementE,
    EpilogueOutputOp::kCount,
    ScatterD
  >::Epilogue;

  /// Define the kernel-level GEMM operator.
  using GemmKernel = kernel::GemmBiasScale<Mma, Epilogue, ThreadblockSwizzle, SplitKSerial>;
};

}  // namespace kernel
}  // namespace gemm
}  // namespace cutlass
