#pragma once

#include "clam/HeapAbstraction.hh"
#include "clam/config.h"
#include "llvm/ADT/StringRef.h"

namespace clam {

/* Dummy heap analysis if no pointer analysis is available */
struct DummyHeapAbstraction : public HeapAbstraction {

  using typename HeapAbstraction::RegionId;
  using typename HeapAbstraction::RegionVec;

  DummyHeapAbstraction() : HeapAbstraction() {}

  HeapAbstraction::ClassId getClassId() const {
    return HeapAbstraction::ClassId::DUMMY;
  }

  Region getRegion(const llvm::Function &, const llvm::Value &) {
    return Region();
  }

  virtual RegionVec getOnlyReadRegions(const llvm::Function &) const override {
    return RegionVec();
  }
  
  virtual RegionVec getModifiedRegions(const llvm::Function &) const override {
    return RegionVec();
  }
  
  virtual RegionVec getNewRegions(const llvm::Function &) const override {
    return RegionVec();
  }

  virtual RegionVec getOnlyReadRegions(const llvm::CallInst &) const override {
    return RegionVec();
  }

  virtual RegionVec getModifiedRegions(const llvm::CallInst &) const override {
    return RegionVec();
  }

  virtual RegionVec getNewRegions(const llvm::CallInst &) const override {
    return RegionVec();
  }

  virtual std::vector<RegionVec> getEquivClassRegions(const llvm::Function &) const override {
    return std::vector<RegionVec>();
  }

  virtual llvm::StringRef getName() const override {
    return "DummyHeapAbstraction";
  }
};

} // namespace clam
