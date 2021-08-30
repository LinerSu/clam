#include "clam/CrabIREmitter.hh"
#include "clam/Support/Debug.hh"

#include "../CfgBuilderUtils.hh"
#include "BndCheck.hh"
#include "MemoryCheckUtils.hh"

#include "llvm/ADT/SmallSet.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"

#include "crab/support/debug.hpp"

using namespace llvm;

namespace clam {

class EmitBndChecksImpl {
  const CrabBuilderParams &m_params;
  crabLitFactory &m_lfac;
  uint32_t &m_assertionId;
  uint32_t m_addedChecks;
  const DataLayout * m_dl;
  using ref_bnd_map_t = llvm::DenseMap<llvm::Value *, std::pair<var_t, var_t>>;
  // map llvm pointer to pair of object size and offset
  ref_bnd_map_t m_ref_bnd_map;

  // Private function here:
  // Get datalayout by given an instruction
  const DataLayout *getInsDatalayout(Instruction &I) {
    if (!m_dl) {
      const llvm::Module &m = *I.getParent()->getParent()->getParent();
      m_dl = &m.getDataLayout();
    }
    return m_dl;
  }

  // Get maximum width in bits through datalayout
  const unsigned getMaxWidthInBits(Instruction &I) {
    if (!m_dl) {
      m_dl = getInsDatalayout(I);
    }
    return m_dl->getMaxPointerSizeInBits();
  }

  // Add bound check assertions with the following pattern:
  // assert(offset >= 0)
  // assert(size > offset)
  // assert(size >= offset + size_of(value))
  // where offset indicates the offset of current pointer
  // the size means the total size of allcation
  // sizeof(value) gets the bit-width of the element
  void addBndCheckAssertion(Instruction &I, const var_t &reg, basic_block_t &bb,
                            bool is_load_ref) {
    std::string log_out = is_load_ref ? "load" : "store";
    if (m_params.check_only_typed_regions &&
        reg.get_type().is_unknown_region()) {
      return;
    }
    // We ignore for now the option
    // m_params.check_only_noncyclic_regions.  To consider it the
    // easiest way is to extend CrabStoreRefOps with information
    // about the HeapAbstraction Region such as whether
    // isSequence(), IsCyclic(), etc.

    llvm::Value *vptr;
    if (LoadInst *loadI = dyn_cast<LoadInst>(&I))
      vptr = loadI->getPointerOperand();
    else if (StoreInst *storeI = dyn_cast<StoreInst>(&I))
      vptr = storeI->getPointerOperand();
    else
      CLAM_ERROR(
          "Expect adding bound check assertions at load/store instrucitons");
    auto it = m_ref_bnd_map.find(vptr);
    if (it == m_ref_bnd_map.end()) {
      CLAM_WARNING("Could not find " << log_out << " ptr from map " << *vptr);
    } else {
      CRAB_LOG("cfg-bnd-check",
               llvm::errs()
                   << log_out << ": check size and offset are in bounds: " << I
                   << "\n");
      var_t size = it->second.first;
      var_t offset = it->second.second;
      Type *elemTy = vptr->getType()->getPointerElementType();
      const llvm::DataLayout *dl = getInsDatalayout(I);
      unsigned size_of = dl->getTypeSizeInBits(elemTy) / 8;
      bb.assertion(offset >= number_t(0), getDebugLoc(&I, m_assertionId++));
      bb.assertion(size >= offset + size_of, getDebugLoc(&I, m_assertionId++));
      m_addedChecks++;
    }
  }

public:
  EmitBndChecksImpl(const CrabBuilderParams &params, crabLitFactory &lfac,
                    uint32_t &assertionId)
      : m_params(params), m_lfac(lfac), m_assertionId(assertionId),
        m_addedChecks(0), m_dl(nullptr) {}

  ~EmitBndChecksImpl() {
    llvm::errs() << "-- Inserted " << m_addedChecks;
    llvm::errs() << " bound checks\n";
  }

  void visitBeforeStore(llvm::StoreInst &I, CrabStoreRefOps &s) {
    addBndCheckAssertion(I, s.getRegion(), s.getBasicBlock(), false);
  }

  void visitBeforeLoad(llvm::LoadInst &I, CrabLoadRefOps &s) {
    addBndCheckAssertion(I, s.getRegion(), s.getBasicBlock(), true);
  }

  void visitAfterLoad(llvm::LoadInst &I, CrabLoadRefOps &s) {
    crab_lit_ref_t lhs = m_lfac.getLit(I);
    basic_block_t &bb = s.getBasicBlock();
    if (lhs->getVar().get_type().is_reference()) {
      var_t ref_obj_sz = m_lfac.mkIntVar(getMaxWidthInBits(I));
      // obtain an overappriximation of deref
      // using havoc to initialize size and offset
      bb.havoc(ref_obj_sz, "approx size when loading a ptr of ptr");
      var_t ref_offset = m_lfac.mkIntVar(getMaxWidthInBits(I));
      bb.havoc(ref_offset, "approx offset when loading a ptr of ptr");

      // add pair to the map by llvm pointer
      Value *ref_v = llvm::cast<llvm::Value>(&I);
      m_ref_bnd_map.insert({ref_v, {ref_obj_sz, ref_offset}});
    }
  }

  // -- alloc site ptr bound utilities construction
  // From LLVM IR to crab IR we perform the following transformation:
  // p := alloc(sz) => p := make_ref(region, sz)
  // We use the idea from fat pointer like by adding two additional variables:
  // p_offset := 0;
  // p_size := sz;
  // where offset indicates the offset of current pointer
  // the size means the total size of allcation
  void visitAfterAlloc(llvm::Instruction &I, const llvm::TargetLibraryInfo &tli,
                       CrabMakeRefOps &s) {
    // llvm::errs() << I << "\n";
    // llvm::errs() << *I.getOperand(0) << "\n";
    basic_block_t &bb = s.getBasicBlock();
    CRAB_LOG("cfg-bnd-check",
             llvm::errs() << "alloc: add size and offset for " << I << "\n");
    llvm::Value *op;
    if (AllocaInst *allocaI = dyn_cast<AllocaInst>(&I))
      op = allocaI->getArraySize();
    else
      op = I.getOperand(0);
    llvm::Value &operand = *op;
    crab_lit_ref_t opd = m_lfac.getLit(operand);
    if (!opd->isInt()) {
      CLAM_WARNING("Unexpected type of alloc size operand");
      return;
    }
    assert(opd->isInt());
    auto Iopd = std::static_pointer_cast<const crabIntLit>(opd);
    unsigned width = Iopd->getBitwidth();
    var_t tmp_obj_sz = m_lfac.mkIntVar(width);
    if (AllocaInst *allocaI = dyn_cast<AllocaInst>(&I)) {
      const llvm::DataLayout *dl = getInsDatalayout(*allocaI);
      unsigned sz = dl->getTypeSizeInBits(allocaI->getAllocatedType()) / 8;
      bb.assign(tmp_obj_sz, m_lfac.getExp(opd) * sz);
    } else {
      bb.assign(tmp_obj_sz, m_lfac.getExp(opd));
    }
    var_t ref_obj_sz = m_lfac.mkIntVar(getMaxWidthInBits(I));
    // convert assigned size to maximum width
    if (getMaxWidthInBits(I) > width)
      bb.zext(tmp_obj_sz, ref_obj_sz);
    else if (getMaxWidthInBits(I) < width)
      bb.truncate(tmp_obj_sz, ref_obj_sz);
    // create a var_t for offset and assign it to zero
    var_t ref_offset = m_lfac.mkIntVar(getMaxWidthInBits(I));
    bb.assign(ref_offset, number_t(0));

    // add pair to the map by llvm pointer
    m_ref_bnd_map.insert({&I, {ref_obj_sz, ref_offset}});
  }

  // -- get element ptr additional utilities construction
  // From LLVM IR to crab IR we perform the following transformation:
  // p := gep(ptr, offset) => p := gep_ref(region, ptr + offset)
  // We use the idea from fat pointer like by adding two additional variables:
  // p_offset := ptr_offset + offset;
  // p_size := ptr_size;
  // where p_offset indicates the offset of element pointer
  // the p_size means the total size of allcation
  // the element ptr should refere the same size as the original one
    void visitAfterGep(llvm::Instruction &I, CrabGepRefOps &s) {
    if (!isa<PHINode>(I) && !isa<BitCastInst>(I) &&
        !isa<GetElementPtrInst>(I)) {
      return;
    }
    lin_exp_t offset = s.getOffset();
    basic_block_t &bb = s.getBasicBlock();
    CRAB_LOG("cfg-bnd-check",
             llvm::errs() << "Gep: add two extra variable for " << I << "\n");
    Value *vlhs = llvm::cast<llvm::Value>(&I);
    Value *vptr = I.getOperand(0);
    unsigned width = getMaxWidthInBits(I);
    if (isa<ConstantPointerNull>(*vptr)) { // Get a constant null ptr
      var_t lhs_obj_sz = m_lfac.mkIntVar(width);
      var_t lhs_offset = m_lfac.mkIntVar(width);
      bb.assign(lhs_obj_sz, number_t(0));
      bb.assign(lhs_offset, number_t(0));
      m_ref_bnd_map.insert({&I, {lhs_obj_sz, lhs_offset}});
    }
    else {
      auto it = m_ref_bnd_map.find(vptr);
      if (it == m_ref_bnd_map.end()) {
        CLAM_WARNING("Could not find gep ptr "
                  << *vptr << " from reference map" << I);
      } else {
        var_t ptr_obj_sz = it->second.first;
        var_t ptr_offset = it->second.second;
        var_t lhs_obj_sz = m_lfac.mkIntVar(width);
        var_t lhs_offset = m_lfac.mkIntVar(width);
        bb.assign(lhs_obj_sz, ptr_obj_sz);
        bb.assign(lhs_offset, offset);
        m_ref_bnd_map.insert({&I, {lhs_obj_sz, lhs_offset}});
      }
    }
  }

  void visitAfterRefSelect(SelectInst &I, CrabSelectRefOps &s) {
    const Value &vtrue = *(I.getTrueValue());
    const Value &vfalse = *(I.getFalseValue());
    const Value &condV = *I.getCondition();
    crab_lit_ref_t c = m_lfac.getLit(condV);
    assert(c);
    crab_lit_ref_t op1 = m_lfac.getLit(vtrue);
    assert(op1);
    crab_lit_ref_t op2 = m_lfac.getLit(vfalse);
    assert(op2);
    basic_block_t &bb = s.getBasicBlock();
    unsigned width = getMaxWidthInBits(I);
    var_t cond = m_lfac.mkIntVar(width);
    bb.zext(c->getVar(), cond); // extend bool into int
    var_t offset_op1 = m_lfac.mkIntVar(width);
    var_t size_op1 = m_lfac.mkIntVar(width);
    var_t offset_op2 = m_lfac.mkIntVar(width);
    var_t size_op2 = m_lfac.mkIntVar(width);
    if (m_lfac.isRefNull(op1)) {
      bb.assign(offset_op1, number_t(0));
      bb.assign(size_op1, number_t(0));
    } else {
      auto it = m_ref_bnd_map.find(&vtrue);
      if (it == m_ref_bnd_map.end()) {
        CLAM_WARNING("Could not find from select ptr "
                     << vtrue << " from reference map" << I);
        return;
      } else {
        size_op1 = it->second.first;
        offset_op1 = it->second.second;
      }
    }
    if (m_lfac.isRefNull(op2)) {
      bb.assign(offset_op2, number_t(0));
      bb.assign(size_op2, number_t(0));
    } else {
      auto it = m_ref_bnd_map.find(&vfalse);
      if (it == m_ref_bnd_map.end()) {
        CLAM_WARNING("Could not find from select ptr "
                     << vfalse << " from reference map" << I);
        return;
      } else {
        size_op2 = it->second.first;
        offset_op2 = it->second.second;
      }
    }
    var_t lhs_obj_sz = m_lfac.mkIntVar(width);
    var_t lhs_offset = m_lfac.mkIntVar(width);
    bb.select(lhs_offset, cond, offset_op1, offset_op2);
    bb.select(lhs_obj_sz, cond, size_op1, size_op2);
    m_ref_bnd_map.insert({&I, {lhs_obj_sz, lhs_offset}});
  }
};

EmitBndChecks::EmitBndChecks(const CrabBuilderParams &params,
                             crabLitFactory &lfac, uint32_t &assertionId) {
  this->m_impl = std::make_unique<EmitBndChecksImpl>(params, lfac, assertionId);
}

EmitBndChecks::~EmitBndChecks() {}

void EmitBndChecks::visitBeforeStore(llvm::StoreInst &I, CrabStoreRefOps &s) {
  m_impl->visitBeforeStore(I, s);
}

void EmitBndChecks::visitBeforeLoad(llvm::LoadInst &I, CrabLoadRefOps &s) {
  m_impl->visitBeforeLoad(I, s);
}

void EmitBndChecks::visitAfterLoad(llvm::LoadInst &I, CrabLoadRefOps &s) {
  m_impl->visitAfterLoad(I, s);
}

void EmitBndChecks::visitAfterAlloc(llvm::Instruction &I,
                                    const llvm::TargetLibraryInfo &tli,
                                    CrabMakeRefOps &s) {
  m_impl->visitAfterAlloc(I, tli, s);
}

void EmitBndChecks::visitAfterGep(llvm::Instruction &I, CrabGepRefOps &s) {
  m_impl->visitAfterGep(I, s);
}

void EmitBndChecks::visitAfterRefSelect(llvm::SelectInst &I,
                                        CrabSelectRefOps &s) {
  m_impl->visitAfterRefSelect(I, s);
}

/* Begin empty implementations */
void EmitBndChecks::visitBeforeBasicBlock(llvm::BasicBlock &BB) {}
void EmitBndChecks::visitAfterBasicBlock(llvm::BasicBlock &BB) {}
void EmitBndChecks::visitBeforeAlloc(llvm::Instruction &I,
                                     const llvm::TargetLibraryInfo &tli,
                                     CrabMakeRefOps &s) {}
void EmitBndChecks::visitBeforeFree(llvm::Instruction &I,
                                    const llvm::TargetLibraryInfo &tli,
                                    CrabRemoveRefOps &s) {}
void EmitBndChecks::visitAfterFree(llvm::Instruction &I,
                                   const llvm::TargetLibraryInfo &tli,
                                   CrabRemoveRefOps &s) {}
void EmitBndChecks::visitBeforeGep(llvm::Instruction &I, CrabGepRefOps &s) {}
void EmitBndChecks::visitAfterStore(llvm::StoreInst &I, CrabStoreRefOps &s) {}
void EmitBndChecks::visitBeforeMemset(llvm::MemSetInst &I, CrabMemsetOps &s) {}
void EmitBndChecks::visitAfterMemset(llvm::MemSetInst &I, CrabMemsetOps &s) {}
void EmitBndChecks::visitBeforeMemTransfer(llvm::MemTransferInst &I,
                                           CrabMemTransferOps &s) {}
void EmitBndChecks::visitAfterMemTransfer(llvm::MemTransferInst &I,
                                          CrabMemTransferOps &s) {}
void EmitBndChecks::visitBeforeRefSelect(llvm::SelectInst &I,
                                         CrabSelectRefOps &s) {}
/* End empty implementations */

} // end namespace clam
