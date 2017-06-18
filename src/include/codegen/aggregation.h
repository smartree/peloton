//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// aggregation.h
//
// Identification: src/include/codegen/aggregation.h
//
// Copyright (c) 2015-17, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <vector>

#include "codegen/codegen.h"
#include "codegen/updateable_storage.h"
#include "codegen/value.h"
#include "planner/aggregate_plan.h"

namespace peloton {
namespace codegen {

//===----------------------------------------------------------------------===//
// This class is responsible for handling the logic around performing
// aggregations. Users first setup the aggregation (through Setup()) with all
// the aggregates they wish calculate. Next, callers provided the initial values
// of all the aggregates using a call to CreateInitialValues(). Each update to
// the set of aggregates is made through AdvanceValues(), with updated values
// for each aggregate. When done, a final call to FinalizeValues() is made to
// collect all the final aggregate values.
//
// Note: the ordering of aggregates and values must be consistent with the
//       ordering provided during Setup().
//===----------------------------------------------------------------------===//
class Aggregation {
 public:
  // Setup the aggregation to handle the provided aggregates
  void Setup(CodeGen &codegen,
             const std::vector<planner::AggregatePlan::AggTerm> &agg_terms,
             bool is_global);

  // Store the initial values of the aggregates into the provided storage space.
  void CreateInitialGlobalValues(CodeGen &codegen,
                                 llvm::Value *storage_space) const;

  // Store the initial values of the aggregates into the provided storage space.
  void CreateInitialValues(CodeGen &codegen, llvm::Value *storage_space,
                           const std::vector<codegen::Value> &initial) const;

  // Advance all the aggregates that are stored in the provided storage space
  // by the values from the provided vector.
  void AdvanceValues(CodeGen &codegen, llvm::Value *storage_space,
                     const std::vector<codegen::Value> &next) const;

  // Compute the final values of all the aggregates stored in the provided
  // storage space, inserting them into the provided output vector.
  void FinalizeValues(CodeGen &codegen, llvm::Value *storage_space,
                      std::vector<codegen::Value> &final_vals) const;

  // Get the total number of bytes needed to store all the aggregates this is
  // configured to store
  uint32_t GetAggregatesStorageSize() const {
    return storage_.GetStorageSize();
  }

  // Get the storage format of the aggregates this class is configured to handle
  const UpdateableStorage &GetAggregateStorage() const { return storage_; }

 private:

  bool IsGlobal() const { return is_global_; }

  //===--------------------------------------------------------------------===//
  // Little struct to map the aggregates we physically store to the higher level
  // aggregates. It is possible that the number of aggregate information structs
  // we keep is not equal to the total number of aggregates the caller has
  // setup. This can occur for two reasons:
  //
  // 1) There are occasions where components of aggregates can be shared
  //    across multiple aggregates.  An example is a SUM(a) and AVG(a). Both
  //    of these will share the summation on the column.
  // 2) Some aggregates decompose into simpler aggregations. An example is AVG()
  //    which we decompose into a SUM() and COUNT().  AVG(), therefore, occupies
  //    three total slots.
  //
  // Storing the mapping from the physical position the aggregate is stored to
  // where the caller expects them allows us to rearrange positions without
  // the caller knowing or caring.
  //===--------------------------------------------------------------------===//
  struct AggregateInfo {
    // The type of aggregate
    ExpressionType aggregate_type;

    // The SQL (data) type of the aggregate
    const type::Type &type;

    // The position in the original (ordered) list of aggregates that this
    // aggregate is stored
    uint32_t source_index;

    // The position in the physical storage space where this aggregate is stored
    uint32_t storage_index;

    // Is this internal? In other words, does the caller know that this
    // aggregate exists?
    bool is_internal;
  };

 private:
  // Advance the value of a specific aggregate, given its next value
  void DoAdvanceValue(CodeGen &codegen, llvm::Value *storage_space,
                      const AggregateInfo &aggregate_info,
                      const codegen::Value &next) const;

 private:
  // Is this a global aggregation?
  bool is_global_;

  // The list of aggregations we handle
  std::vector<AggregateInfo> aggregate_infos_;

  // The storage format we use to store values
  UpdateableStorage storage_;
};

}  // namespace codegen
}  // namespace peloton