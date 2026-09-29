#include "clam/config.h"

/**
 * Heap abstraction based on sea-dsa (https://github.com/seahorn/sea-dsa).
 *
 * The implementation currently ignores InvokeInst and assumes that
 * the callgraph is complete.
 */

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/ImmutableSet.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

#include "seadsa/AllocWrapInfo.hh"
#include "seadsa/CallSite.hh"
#include "seadsa/DsaLibFuncInfo.hh"
#include "seadsa/Global.hh"
#include "seadsa/Graph.hh"

#include "SeaDsaHeapAbstractionUtils.hh"
#include "SeaDsaToRegion.hh"
#include "clam/HeapAbstraction.hh"
#include "clam/SeaDsaHeapAbstraction.hh"
#include "clam/Support/Debug.hh"
#include "crab/support/debug.hpp"
#include "seadsa/support/Stats.hh"

#include <algorithm>
#include <memory>
#include <unordered_map>

namespace clam {

using namespace seadsa;
using namespace llvm;

// The cells of a graph whose tags a CrabIR statement can read or write
// although no load or store accesses them: the pointer operands of
// memcpy, memmove and memset and of the crab intrinsics
// (__CRAB_intrinsic_*: add_tag, check_*, move_tag, remove_tag, ...).
struct TagOperandCells {
  // for each node, the sorted offsets of those cells
  llvm::DenseMap<const Node *, std::vector<unsigned>> offsets;
  // the nodes whose tags such an operand may write
  llvm::DenseSet<const Node *> written;
};

class SeaDsaHeapAbstractionImpl {
  // XXX: We should use seadsa::Graph::SetFactory.
  // We copy here the definition of seadsa::Graph::SetFactory so
  // that we don't need to include Graph.hh
  using Set = llvm::ImmutableSet<llvm::Type *>;
  using SetFactory = typename Set::Factory;
  using callsite_map_t =
      llvm::DenseMap<const llvm::CallInst *, std::vector<std::pair<Region, bool>>>;

  Region mkRegion(const seadsa::Cell &c, RegionInfo ri);

  HeapAbstraction::RegionId getId(const seadsa::Cell &c);

  void initialize(const llvm::Module &M);

  // compute and cache the set of read, mod and new nodes of a whole
  // function such that mod nodes are a subset of the read nodes and
  // the new nodes are disjoint from mod nodes.
  void computeReadModNewNodes(const llvm::Function &f);

  // Compute and cache the set of read, mod and new nodes of a
  // callsite such that mod nodes are a subset of the read nodes and
  // the new nodes are disjoint from mod nodes.
  void computeReadModNewNodesFromCallSite(const llvm::CallInst &I,
                                          callsite_map_t &accessed,
                                          callsite_map_t &mods,
                                          callsite_map_t &news);

  void computeEquivClasses(const llvm::Function &f);

  // Record, in the graph of each function of M, the cells of the
  // pointer operands of memory intrinsics and crab intrinsics.
  void computeTagOperandCells(const llvm::Module &M);

  // The tag-operand cells of G (none if G has no such operand).
  const TagOperandCells &getTagOperandCells(const seadsa::Graph &G) const;

  const llvm::Value *getSingleton(HeapAbstraction::RegionId region) const;

public:
  // This constructor creates and owns a sea-dsa GlobalAnalysis instance and
  // run it on M.
  SeaDsaHeapAbstractionImpl(const llvm::Module &M, llvm::CallGraph &cg,
                            const llvm::TargetLibraryInfoWrapperPass &tli,
                            const seadsa::AllocWrapInfo &alloc_wrap_info,
                            const seadsa::DsaLibFuncInfo &spec_graph_info,
			    SeaDsaHeapAbstractionParams params);

  // This constructor takes an existing sea-dsa Global Analysis instance.
  // It doesn't own it.
  SeaDsaHeapAbstractionImpl(const llvm::Module &M, seadsa::GlobalAnalysis &dsa,
			    SeaDsaHeapAbstractionParams params);

  ~SeaDsaHeapAbstractionImpl();

  seadsa::GlobalAnalysis *getSeaDsa() { return m_dsa; }

  const seadsa::GlobalAnalysis *getSeaDsa() const { return m_dsa; }

  // Use F and V to get sea-dsa cell associated to it.
  Region getRegion(const llvm::Function &F, const llvm::Value &V);

  // Use F and V to get the sea-dsa node associated to V and extracts
  // the region associated to nodes's field offset if any.
  Region getRegion(const llvm::Function &F, const llvm::Value &V,
                   unsigned offset, const llvm::Type &AccessedType);

  HeapAbstraction::RegionVec getOnlyReadRegions(const llvm::Function &F) const;

  HeapAbstraction::RegionVec getModifiedRegions(const llvm::Function &F) const;

  HeapAbstraction::RegionVec getNewRegions(const llvm::Function &F) const;

  HeapAbstraction::RegionVec getOnlyReadRegions(const llvm::CallInst &I) const;

  HeapAbstraction::RegionVec getModifiedRegions(const llvm::CallInst &I) const;

  HeapAbstraction::RegionVec getNewRegions(const llvm::CallInst &I) const;

  std::vector<HeapAbstraction::RegionVec>
  getEquivClassRegions(const llvm::Function &F) const;

private:
  seadsa::GlobalAnalysis *m_dsa;
  std::unique_ptr<SetFactory> m_fac;
  const llvm::DataLayout &m_dl;

  struct CellIdKey {
    const seadsa::Node *node;
    unsigned offset;

    bool operator==(const CellIdKey &o) const {
      return node == o.node && offset == o.offset;
    }
  };

  struct CellIdKeyHash {
    std::size_t operator()(const CellIdKey &k) const {
      std::size_t seed = std::hash<const seadsa::Node *>{}(k.node);
      seed ^= std::hash<unsigned>{}(k.offset) + 0x9e3779b9 + (seed << 6) +
              (seed >> 2);
      return seed;
    }
  };

  /// Map each canonical SeaDsa cell to a unique region id.
  std::unordered_map<CellIdKey, HeapAbstraction::RegionId, CellIdKeyHash>
      m_cell_ids;
  /// reverse map
  std::unordered_map<HeapAbstraction::RegionId, const seadsa::Node *>
      m_rev_node_ids;
  HeapAbstraction::RegionId m_next_id;
  SeaDsaHeapAbstractionParams m_params;

  // read or modified regions reachable from inputs or globals
  llvm::DenseMap<const llvm::Function *, HeapAbstraction::RegionVec>
      m_func_accessed;
  // modified regions reachable from inputs or globals
  llvm::DenseMap<const llvm::Function *, HeapAbstraction::RegionVec>
      m_func_mods;
  // regions reachable only from return parameters
  llvm::DenseMap<const llvm::Function *, HeapAbstraction::RegionVec>
      m_func_news;
  llvm::DenseMap<const llvm::CallInst *, HeapAbstraction::RegionVec>
      m_callsite_accessed;
  llvm::DenseMap<const llvm::CallInst *, HeapAbstraction::RegionVec>
      m_callsite_mods;
  llvm::DenseMap<const llvm::CallInst *, HeapAbstraction::RegionVec>
      m_callsite_news;

  // Group together all the regions originated from the same node
  llvm::DenseMap<const llvm::Function *,
                 std::vector<HeapAbstraction::RegionVec>>
      m_func_equiv_class_regions;

  // Tag-operand cells per graph (one graph for all functions under a
  // context-insensitive sea-dsa)
  llvm::DenseMap<const seadsa::Graph *, TagOperandCells> m_tag_operand_cells;
  TagOperandCells m_no_tag_operand_cells;
};

Region SeaDsaHeapAbstractionImpl::mkRegion(const Cell &c, RegionInfo ri) {
  auto id = getId(c);
  return Region(id, ri, getSingleton(id));
}

HeapAbstraction::RegionId SeaDsaHeapAbstractionImpl::getId(const Cell &c) {
  const Node *n = c.getNode();
  unsigned offset = c.getOffset();

  CellIdKey key{n, offset};
  auto it = m_cell_ids.find(key);
  if (it != m_cell_ids.end()) {
    return it->second;
  }

  HeapAbstraction::RegionId id = m_next_id++;
  m_cell_ids.emplace(key, id);

  // XXX: we only have the reverse map for the offset 0.  That's
  // fine because we use this map only in getSingleton which can
  // only succeed if offset 0.
  if (offset == 0 && n->getUniqueScalar()) {
    m_rev_node_ids[id] = n;
  }

  return id;
}

static std::vector<unsigned> extractFields(const Node *n,
                                           const TagOperandCells &tagOperands,
                                           bool forceZeroOffset = true) {
  std::vector<unsigned> fields;
  if (n->isOffsetCollapsed()) {
    fields.push_back(0);
  } else {
    for (auto &kv : n->types()) {
      fields.push_back(kv.first);
    }
#ifdef SEA_BUILD_IDSA
    // I-DSA: a partially-collapsed (interval) cell carries no accessed
    // types, but it is a memory region like any typed field. It must be
    // part of the function's read/mod/new regions, otherwise the region is
    // only created on demand inside each function and whatever a callee
    // writes into it (e.g. a memcpy into a flexible array member) is lost
    // at the call boundary: the caller's copy of the region never sees the
    // tag. Interval cells are keyed by their start offset (the canonical
    // offset of every access inside the interval).
    for (const auto &ck : n->getCollapsedCells()) {
      fields.push_back(ck.getOffset());
    }
#endif
    if (fields.empty() && forceZeroOffset) {
      fields.push_back(0);
    }
    // Likewise, a cell that no load or store accesses carries no accessed
    // type, but a memcpy/memmove/memset or a crab intrinsic (add_tag,
    // check_*, move_tag, remove_tag, ...) can read or write its tags
    // through a pointer to it. Such a cell must be a function-interface
    // region as well: otherwise the region is only created on demand
    // (getRegion) inside each function, a tag that a callee writes into
    // it is dropped at the return, and a callee that reads it sees a
    // freshly initialized region instead of the caller's. (If no load or
    // store accesses the node at all, the callers below admit it through
    // these cells: sea-dsa marks neither reads nor modifications for the
    // operands of external calls, crab intrinsics included.)
    //
    // Other cells that pointers reach (e.g. only through gep_ref or
    // make_ref) are left out: no statement reads or writes their tags.
    // A memcpy that spans such a cell then has no region to match those
    // bytes to, as for any byte range that no cell covers.
    auto it = tagOperands.offsets.find(n);
    if (it != tagOperands.offsets.end()) {
      fields.insert(fields.end(), it->second.begin(), it->second.end());
    }
  }
  std::sort(fields.begin(), fields.end());
  fields.erase(std::unique(fields.begin(), fields.end()), fields.end());
  return fields;
}

// Crab intrinsics are external declarations named __CRAB_intrinsic_<name>
// (as isCrabIntrinsic in CfgBuilderUtils). Return <name>, or an empty
// string for any other callee.
static StringRef getCrabIntrinsicSuffix(const CallInst &CI) {
  const Function *callee = dyn_cast<Function>(
      CI.getCalledOperand()->stripPointerCastsAndAliases());
  if (!callee || !callee->isDeclaration()) {
    return StringRef();
  }
  static const StringRef prefix("__CRAB_intrinsic_");
  StringRef name = callee->getName();
  if (!name.startswith(prefix)) {
    return StringRef();
  }
  return name.drop_front(prefix.size());
}

void SeaDsaHeapAbstractionImpl::computeTagOperandCells(
    const llvm::Module &M) {
  if (!m_dsa || m_params.precision_level != CrabBuilderPrecision::MEM) {
    // Such cells are untyped, and only the MEM level tracks untyped
    // regions. Below it, the function would drop the region from its
    // parameters (isTrackedRegion) while a callsite whose caller cell is
    // typed would keep it: the parameter lists would no longer match.
    return;
  }
  // Graph::getCell creates a node for a global value on every call, so
  // the cells of globals are taken from the graph's own map.
  llvm::DenseMap<const Graph *, llvm::DenseMap<const Value *, const Cell *>>
      globalCells;
  for (const Function &F : M) {
    if (F.isDeclaration() || !m_dsa->hasGraph(F)) {
      continue;
    }
    Graph &G = m_dsa->getGraph(F);
    auto git = globalCells.find(&G);
    if (git == globalCells.end()) {
      git = globalCells.try_emplace(&G).first;
      for (auto &kv : G.globals()) {
        git->second[kv.first] = kv.second.get();
      }
    }
    const auto &globals = git->second;
    TagOperandCells &cells = m_tag_operand_cells[&G];
    auto addOperand = [&G, &globals, &cells](const Value *V, bool isWritten) {
      if (!V->getType()->isPointerTy()) {
        return;
      }
      const Value *S = V->stripPointerCasts();
      const Cell *c = nullptr;
      if (isa<GlobalValue>(S)) {
        auto it = globals.find(S);
        if (it != globals.end()) {
          c = it->second;
        }
      } else if (G.hasCell(*S)) {
        c = &G.getCell(*S);
      }
      if (!c || c->isNull()) {
        return;
      }
      // Cell::getOffset is the canonical offset (the one getRegion uses)
      cells.offsets[c->getNode()].push_back(c->getOffset());
      if (isWritten) {
        cells.written.insert(c->getNode());
      }
    };
    for (const Instruction &I : instructions(F)) {
      if (const MemTransferInst *MTI = dyn_cast<MemTransferInst>(&I)) {
        addOperand(MTI->getRawDest(), true);
        addOperand(MTI->getRawSource(), false);
      } else if (const MemSetInst *MSI = dyn_cast<MemSetInst>(&I)) {
        addOperand(MSI->getRawDest(), true);
      } else if (const CallInst *CI = dyn_cast<CallInst>(&I)) {
        StringRef name = getCrabIntrinsicSuffix(*CI);
        if (name.empty()) {
          continue;
        }
        // Only these intrinsics never write tags. move_tag(p1, p2) writes
        // p2's tags from p1's; any other one is taken as a writer.
        bool readsOnly = name == "check_does_not_have_tag" ||
                         name == "check_has_tag" || name == "print_tags" ||
                         name == "is_dereferenceable" ||
                         name == "is_unfreed_or_null";
        for (unsigned i = 0, e = CI->arg_size(); i < e; ++i) {
          bool isWritten =
              !readsOnly && !(name == "move_tag" && i == 0);
          addOperand(CI->getArgOperand(i), isWritten);
        }
      }
    }
  }
  for (auto &kv : m_tag_operand_cells) {
    for (auto &nkv : kv.second.offsets) {
      std::vector<unsigned> &v = nkv.second;
      std::sort(v.begin(), v.end());
      v.erase(std::unique(v.begin(), v.end()), v.end());
    }
  }
}

const TagOperandCells &
SeaDsaHeapAbstractionImpl::getTagOperandCells(const Graph &G) const {
  auto it = m_tag_operand_cells.find(&G);
  return it != m_tag_operand_cells.end() ? it->second
                                         : m_no_tag_operand_cells;
}

// A node enters the regions of a function if some access reads or
// modifies it, if the function returns it, or (below) if it holds a
// tag-operand cell: crab intrinsics are external calls, which sea-dsa
// does not mark as reads or modifications, so a buffer that only
// external calls and intrinsics touch is neither read nor modified.
// Such a node gets no offset-0 region by default, only its tag-operand
// cells. A node is modified if sea-dsa says so or if a tag-operand cell
// of it may be written.
static bool isNodeAccessed(const Node *n, bool isRetReach,
                           const TagOperandCells &tagOperands) {
  return isRetReach || n->isRead() || n->isModified() ||
         tagOperands.offsets.count(n) > 0;
}

static bool isNodeModified(const Node *n, const TagOperandCells &tagOperands) {
  return n->isModified() || tagOperands.written.count(n) > 0;
}

static std::vector<unsigned>
extractAccessedFields(const Node *n, bool isRetReach,
                      const TagOperandCells &tagOperands) {
  bool onlyTagOperands = !isRetReach && !n->isRead() && !n->isModified();
  return extractFields(n, tagOperands, !onlyTagOperands);
}

// compute and cache the set of read, mod and new reachable nodes from
// globals and function's parameters and returns such that mod nodes
// are a subset of the read nodes and the new nodes are disjoint from
// mod nodes.
void SeaDsaHeapAbstractionImpl::computeReadModNewNodes(
    const llvm::Function &f) {

  if (!m_dsa || !(m_dsa->hasGraph(f))) {
    return;
  }

  Graph &G = m_dsa->getGraph(f);
  // hook: skip shadow mem functions created by SeaHorn
  // We treat them as readnone functions
  if (f.getName().startswith("shadow.mem")) {
    return;
  }
  CRAB_LOG("heap-abs-regions", llvm::errs()
                                   << "======== Begin ========= \n Computing "
                                      "read/mod/new regions for function "
                                   << f.getName() << "\n";);

  seadsa_heap_abs_impl::NodeSet reach, retReach;
  seadsa_heap_abs_impl::argReachableNodes(f, G, reach, retReach);

  CRAB_LOG(
      "heap-abs-regions", llvm::errs() << "After argReachableNodes call:\n";
      for (const Node *n
           : seadsa_heap_abs_impl::orderedNodes(reach)) {
        llvm::errs() << "Node reachable: " << n << "\n";
      } for (const Node *n
             : seadsa_heap_abs_impl::orderedNodes(retReach)) {
        llvm::errs() << "Node reachable from return: " << n << "\n";
      });

  HeapAbstraction::RegionVec reads, mods, news;
  std::vector<HeapAbstraction::RegionVec> equivClasses;
  const TagOperandCells &tagOperands = getTagOperandCells(G);
  for (const Node *n : seadsa_heap_abs_impl::orderedNodes(reach)) {
    bool isRetReach = retReach.count(n) > 0;

    if (!isNodeAccessed(n, isRetReach, tagOperands)) {
      continue;
    }
    bool isModified = isNodeModified(n, tagOperands);

    // Extract all fields from the node
    std::vector<unsigned> fields =
        extractAccessedFields(n, isRetReach, tagOperands);
    // Create a region for each node's field
    for (auto field : fields) {
      Cell c(const_cast<Node *>(n), field);
      RegionInfo r_info =
          SeaDsaToRegion(c, m_dl,
			 m_params.disambiguate_unknown,
                         m_params.disambiguate_ptr_cast,
			 m_params.disambiguate_external);
      Region rgn = mkRegion(c, r_info);
      if (!isRetReach) {
        // reachable from function arguments or globals
        reads.push_back(rgn);
        if (isModified) {
          mods.push_back(rgn);
        }
      } else {
        // not reachable from function arguments or globals but
        // reachable from return
        // if (n->isModified()) {
        news.push_back(rgn);
        //}
      }
    } // end for each field
  }   // end for each node
  CRAB_LOG(
      "heap-abs-regions",
      llvm::errs() << "### HEAP_ABS (declaration): " << f.getName() << " ###\n";
      llvm::errs() << "Read regions reachable from arguments and globals {";
      for (auto &r
           : reads) { llvm::errs() << r << ";"; } llvm::errs()
      << "}\n";
      llvm::errs() << "Modified regions reachable from arguments and globals {";
      for (auto &r
           : mods) { llvm::errs() << r << ";"; } llvm::errs()
      << "}\n";
      llvm::errs() << "New regions (only reachable from returns) {";
      for (auto &r
           : news) { llvm::errs() << r << ";"; } llvm::errs()
      << "}\n";
      llvm::errs() << "======== END ========\n";);

  m_func_accessed[&f] = std::move(reads);
  m_func_mods[&f] = std::move(mods);
  m_func_news[&f] = std::move(news);
}

/*
 * Remember regions originated from the same seadsa node.
 *
 * This method iterates *again* over inputs and outputs. Part of the
 * work can be done in computeReadModNewNode but I prefer to redo the
 * work and keep it clean and simple.
 */
void SeaDsaHeapAbstractionImpl::computeEquivClasses(const llvm::Function &f) {
  if (!m_dsa || !(m_dsa->hasGraph(f))) {
    return;
  }

  Graph &G = m_dsa->getGraph(f);
  if (f.getName().startswith("shadow.mem")) {
    return;
  }

  // to store the equivalence classes
  std::vector<HeapAbstraction::RegionVec> equivClasses;

  // reachable from inputs and outputs
  seadsa_heap_abs_impl::NodeSet reach, retReach;
  seadsa_heap_abs_impl::argReachableNodes(f, G, reach, retReach);
  // and also reachable from locals
  for (auto &kv : G.scalars()) {
    if (const Node *n = kv.second->getNode()) {
      seadsa_heap_abs_impl::markReachableNodes(n, reach);
    }
  }

  const TagOperandCells &tagOperands = getTagOperandCells(G);
  for (const Node *n : seadsa_heap_abs_impl::orderedNodes(reach)) {
    bool isRetReach = retReach.count(n) > 0;
    if (!isNodeAccessed(n, isRetReach, tagOperands)) {
      continue;
    }
    HeapAbstraction::RegionVec nodeRgns;
    std::vector<unsigned> fields =
        extractAccessedFields(n, isRetReach, tagOperands);
    for (auto field : fields) {
      Cell c(const_cast<Node *>(n), field);
      RegionInfo r_info =
          SeaDsaToRegion(c, m_dl,
			 m_params.disambiguate_unknown,
                         m_params.disambiguate_ptr_cast,
			 m_params.disambiguate_external);
      nodeRgns.emplace_back(mkRegion(c, r_info));
    }
    equivClasses.emplace_back(std::move(nodeRgns));
  }

  m_func_equiv_class_regions[&f] = std::move(equivClasses);
}

// Compute and cache the set of read, mod and new nodes of a
// callsite such that mod nodes are a subset of the read nodes and
// the new nodes are disjoint from mod nodes.
void SeaDsaHeapAbstractionImpl::computeReadModNewNodesFromCallSite(
    const llvm::CallInst &I, callsite_map_t &accessed_map,
    callsite_map_t &mods_map, callsite_map_t &news_map) {

  auto checkConsistency = [this](const RegionInfo &r1, const RegionInfo &r2) {
    bool res = r1.hasCompatibleType(r2);
    if (m_params.precision_level == CrabBuilderPrecision::SINGLETON_MEM) {
      // seadsa does not guarantee that callsites agree with their
      // corresponding function parameters on dsa node flags.
      res &= (r1.isHeap() == r2.isHeap()); 
    }
    return res;
  };
  
  if (!m_dsa) {
    return;
  }

  /// ignore inline assembly
  if (I.isInlineAsm()) {
    return;
  }
  
  DsaCallSite CS(I);

  if (!CS.getCallee()) {
    CRAB_LOG("heap-abs-regions", llvm::errs()
                                     << "HEAP_ABS: skipped " << I << "\n";);
    return;
  }

  // hook: skip shadow mem functions created by SeaHorn
  // We treat them as readnone functions
  if (CS.getCallee()->getName().startswith("shadow.mem")) {
    return;
  }

  const Function &CalleeF = *CS.getCallee();
  const Function &CallerF = *CS.getCaller();
  if (!m_dsa->hasGraph(CalleeF)) {
    return;
  }
  if (!m_dsa->hasGraph(CallerF)) {
    return;
  }

  Graph &callerG = m_dsa->getGraph(CallerF);
  Graph &calleeG = m_dsa->getGraph(CalleeF);

  // -- compute callee nodes reachable from arguments and returns
  seadsa_heap_abs_impl::NodeSet reach, retReach;
  seadsa_heap_abs_impl::argReachableNodes(CalleeF, calleeG, reach, retReach);

  // -- compute mapping between callee and caller graphs
  SimulationMapper simMap;
  Graph::computeCalleeCallerMapping(CS, calleeG, callerG, simMap);

  std::vector<std::pair<Region, bool>> reads, mods, news;
  // the callee's nodes and fields, as computeReadModNewNodes(CalleeF)
  // enumerates them
  const TagOperandCells &tagOperands = getTagOperandCells(calleeG);
  for (const Node *n : seadsa_heap_abs_impl::orderedNodes(reach)) {
    bool isRetReach = retReach.count(n) > 0;

    if (!isNodeAccessed(n, isRetReach, tagOperands)) {
      continue;
    }
    bool isModified = isNodeModified(n, tagOperands);

    // Extract all fields from the node
    std::vector<unsigned> fields =
        extractAccessedFields(n, isRetReach, tagOperands);

    for (auto field : fields) {
      Cell calleeC(const_cast<Node *>(n), field);
      // Map the callee node to the node in the caller's callsite
      Cell callerC = simMap.get(calleeC);
      if (callerC.isNull()) {
        CLAM_ERROR("caller cell cannot be mapped to callee cell");
      }
      RegionInfo callerRI =
          SeaDsaToRegion(callerC, m_dl,
			 m_params.disambiguate_unknown,
                         m_params.disambiguate_ptr_cast,
			 m_params.disambiguate_external);
      RegionInfo calleeRI =
          SeaDsaToRegion(calleeC, m_dl,
			 m_params.disambiguate_unknown,
                         m_params.disambiguate_ptr_cast,
			 m_params.disambiguate_external);
      bool isConsistent = checkConsistency(callerRI, calleeRI);
      
      Region rgn(mkRegion(callerC, callerRI));      
      if (!isRetReach) {
        reads.push_back({rgn, isConsistent});
        if (isModified) {
          mods.push_back({rgn, isConsistent});
        }
      } else {
        // if (n->isModified()) {
        news.push_back({rgn, isConsistent});
        //}
      }
    }
  }

  accessed_map[&I] = reads;
  mods_map[&I] = mods;
  news_map[&I] = news;

  CRAB_LOG(
      "heap-abs-regions2", llvm::errs()
                               << "### HEAP_ABS (callsite): " << I << " ###\n";
      llvm::errs() << "Read regions at caller mapped to "
                   << "those reachable from callee's arguments and globals {";
      for (auto &r
           : reads) {
        llvm::errs() << r.first << "#" << r.second << ";";
      } llvm::errs()
      << "}\n";
      llvm::errs() << "Modified regions at caller mapped to "
                   << "those reachable from calle's arguments and globals {";
      for (auto &r
           : mods) {
        llvm::errs() << r.first << "#" << r.second << ";";
      } llvm::errs()
      << "}\n";
      llvm::errs()
      << "New regions at the caller (only reachable from callee's returns) {";
      for (auto &r
           : news) {
        llvm::errs() << r.first << "#" << r.second << ";";
      } llvm::errs()
      << "}\n";);
}
  
// Pre-compute all the information per function and callsites
void SeaDsaHeapAbstractionImpl::initialize(const llvm::Module &M) {

  CRAB_LOG(
      "heap-abs",
      llvm::errs() << "========= HeapAbstraction using sea-dsa =========\n";);

  CRAB_LOG(
      "heap-abs-graphs", for (auto &F
                              : M) {
        if (m_dsa->hasGraph(F)) {
          llvm::errs() << "#### " << F.getName() << "####\n";
          auto &G = m_dsa->getGraph(F);
          G.write(llvm::errs());
          llvm::errs() << "\n";
        }
      });

  computeTagOperandCells(M);

  callsite_map_t CSAccessed, CSMods, CSNews;
  // For the sanity check implemented below
  DenseMap<const Function *, std::vector<DsaCallSite>> FunctionCallsMap;

  std::vector<const Function *> functions;
  for (auto const &F : M) {
    functions.push_back(&F);
  }
  std::sort(functions.begin(), functions.end());

  for (const Function *FF : functions) {
    auto &F = *FF;
    computeReadModNewNodes(F);
    computeEquivClasses(F);
    auto InstIt = inst_begin(F), InstItEnd = inst_end(F);
    for (; InstIt != InstItEnd; ++InstIt) {
      if (const CallInst *CI = dyn_cast<llvm::CallInst>(&*InstIt)) {
        computeReadModNewNodesFromCallSite(*CI, CSAccessed, CSMods, CSNews);

        // For the sanity check implemented below
        DsaCallSite CS(*CI);
        if (const Function *calleeF = CS.getCallee()) {
          if (!calleeF->empty()) {
            FunctionCallsMap[calleeF].push_back(CS);
          }
        }
      }
    }
  }

  /// Sanity check: compatibility check between function regions and
  /// all its callsites' regions. The check should always pass. Note
  /// that whether two region's types are compatible is defined by
  /// "hasCompatibleType".
  for (auto const &F : M) {
    HeapAbstraction::RegionVec &readsF = m_func_accessed[&F];
    HeapAbstraction::RegionVec &modsF = m_func_mods[&F];
    HeapAbstraction::RegionVec &newsF = m_func_news[&F];

    // Initialized to true (i.e., all regions are consistent until the
    // opposite is proven)
    std::vector<bool> readsB(readsF.size(), true);
    std::vector<bool> modsB(modsF.size(), true);
    std::vector<bool> newsB(newsF.size(), true);

    /// First pass: for each memory region we check whether caller and
    /// callee agree on it.
    std::vector<const CallInst *> FCalls;
    for (DsaCallSite CS : FunctionCallsMap[&F]) {
      const CallInst *CI = cast<CallInst>(CS.getInstruction());
      FCalls.push_back(CI);

      std::vector<std::pair<Region, bool>> &readsC = CSAccessed[CI];
      std::vector<std::pair<Region, bool>> &modsC = CSMods[CI];
      std::vector<std::pair<Region, bool>> &newsC = CSNews[CI];

      // Check that at the beginning caller and callee agree on the
      // number of memory regions, otherwwise there is nothing we can do.
      if (readsC.size() != readsF.size()) {
        CLAM_ERROR("Different num of read and modified regions between "
                   "callsite and its callee "
                   << F.getName() << " and callsite=" << *CI);
      }
      if (modsC.size() != modsF.size()) {
        CLAM_ERROR(
            "Different num of modified regions between callsite and its callee "
            << F.getName() << " and callsite=" << *CI);
      }
      if (newsC.size() != newsF.size()) {
        CLAM_ERROR(
            "Different num of new regions between callsite and its callee "
            << F.getName() << " and callsite=" << *CI);
      }

      // Keep track of inconsistent memory regions (i.e., regions on
      // which caller and callee disagree). Note that there are
      // legitimate cases where they can disagree.
      for (unsigned i = 0, e = readsC.size(); i < e; i++) {
        readsB[i] = readsB[i] && readsC[i].second;
	CRAB_LOG("heap-abs-regions",
		  if (!readsC[i].second) {
		    llvm::errs() << "Caller and callee disagree on some read region type\n"
		                 << "\tCallsite=" << *CI << "\n"
		                 << "\tRegion at caller=" << readsF[i] << "\n"
		                 << "\tRegion at callee=" << readsC[i].first << "\n";
		  });
		  
      }
      for (unsigned i = 0, e = modsC.size(); i < e; i++) {
        modsB[i] = modsB[i] && modsC[i].second;
	CRAB_LOG("heap-abs-regions",
		  if (!modsC[i].second) {
		    llvm::errs() << "Caller and callee disagree on some modified region type\n"
		                 << "\tCallsite=" << *CI << "\n"
		                 << "\tRegion at caller=" << modsF[i] << "\n"
		                 << "\tRegion at callee=" << modsC[i].first << "\n";
		  });
      }
      for (unsigned i = 0, e = newsC.size(); i < e; i++) {
        newsB[i] = newsB[i] && newsC[i].second;
	CRAB_LOG("heap-abs-regions",
		  if (!newsC[i].second) {
		    llvm::errs() << "Caller and callee disagree on some new region type\n"
		                 << "\tCallsite=" << *CI << "\n"
		                 << "\tRegion at caller=" << newsF[i] << "\n"
		                 << "\tRegion at callee=" << newsC[i].first << "\n";
		  });
      }
    }

    /// Second phase: cache final regions.
    HeapAbstraction::RegionVec readsF_out, modsF_out, newsF_out;
    for (unsigned i = 0, e = readsB.size(); i < e; i++) {
      if (readsB[i]) {
        readsF_out.push_back(readsF[i]);
      }
    }
    for (unsigned i = 0, e = modsB.size(); i < e; i++) {
      if (modsB[i]) {
        modsF_out.push_back(modsF[i]);
      }
    }
    for (unsigned i = 0, e = newsB.size(); i < e; i++) {
      if (newsB[i]) {
        newsF_out.push_back(newsF[i]);
      }
    }

    m_func_accessed[&F] = readsF_out;
    m_func_mods[&F] = modsF_out;
    m_func_news[&F] = newsF_out;

    while (!FCalls.empty()) {
      const CallInst *CI = FCalls.back();
      FCalls.pop_back();
      HeapAbstraction::RegionVec readsC_out, modsC_out, newsC_out;
      std::vector<std::pair<Region, bool>> &readsC = CSAccessed[CI];
      std::vector<std::pair<Region, bool>> &modsC = CSMods[CI];
      std::vector<std::pair<Region, bool>> &newsC = CSNews[CI];
      for (unsigned i = 0, e = readsB.size(); i < e; i++) {
        if (readsB[i]) {
          readsC_out.push_back(readsC[i].first);
        }
      }
      for (unsigned i = 0, e = modsB.size(); i < e; i++) {
        if (modsB[i]) {
          modsC_out.push_back(modsC[i].first);
        }
      }
      for (unsigned i = 0, e = newsB.size(); i < e; i++) {
        if (newsB[i]) {
          newsC_out.push_back(newsC[i].first);
        }
      }

      m_callsite_accessed[CI] = readsC_out;
      m_callsite_mods[CI] = modsC_out;
      m_callsite_news[CI] = newsC_out;
      CRAB_LOG(
          "heap-abs-regions", llvm::errs()
                                  << "### HEAP_ABS: " << *CI << " ###\n";
          llvm::errs()
          << "Read regions at caller mapped to "
          << "those reachable from callee's arguments and globals {";
          for (auto &r
               : readsC_out) { llvm::errs() << r << ";"; } llvm::errs()
          << "}\n";
          llvm::errs()
          << "Modified regions at caller mapped to "
          << "those reachable from calle's arguments and globals {";
          for (auto &r
               : modsC_out) { llvm::errs() << r << ";"; } llvm::errs()
          << "}\n";
          llvm::errs() << "New regions at the caller (only reachable from "
                          "callee's returns) {";
          for (auto &r
               : newsC_out) { llvm::errs() << r << ";"; } llvm::errs()
          << "}\n";);
    }
  }
}

SeaDsaHeapAbstractionImpl::SeaDsaHeapAbstractionImpl(
    const llvm::Module &M, llvm::CallGraph &cg,
    const llvm::TargetLibraryInfoWrapperPass &tli,
    const seadsa::AllocWrapInfo &alloc_info,
    const seadsa::DsaLibFuncInfo &spec_graph_info,
    SeaDsaHeapAbstractionParams params)
    : m_dsa(nullptr), m_fac(new SetFactory()), m_dl(M.getDataLayout()),
      m_next_id(1), m_params(params) {

  // -- Run sea-dsa
  if (!m_params.is_context_sensitive) {
    m_dsa = new seadsa::ContextInsensitiveGlobalAnalysis(
        m_dl, *(const_cast<llvm::TargetLibraryInfoWrapperPass *>(&tli)),
        alloc_info, spec_graph_info, cg, *m_fac, false);
  } else {
    m_dsa = new seadsa::ContextSensitiveGlobalAnalysis(
        m_dl, *(const_cast<llvm::TargetLibraryInfoWrapperPass *>(&tli)),
        alloc_info, spec_graph_info, cg, *m_fac);
  }
  m_dsa->runOnModule(const_cast<Module &>(M));

  {
    SEADSA_SCOPED_STATS("clam.dsa.initialize", 1);
    initialize(M);
  }
}

SeaDsaHeapAbstractionImpl::SeaDsaHeapAbstractionImpl(
    const llvm::Module &M, seadsa::GlobalAnalysis &dsa,
    SeaDsaHeapAbstractionParams params)
    : m_dsa(&dsa), m_fac(nullptr), m_dl(M.getDataLayout()), m_next_id(1),
      m_params(params) {
  initialize(M);
}

SeaDsaHeapAbstractionImpl::~SeaDsaHeapAbstractionImpl() {
  if (m_fac) {
    // if m_fac is not null we know that we own m_dsa
    delete m_dsa;
  }
}

// f is used to know in which Graph we should search for V
Region SeaDsaHeapAbstractionImpl::getRegion(const llvm::Function &fn,
                                            const llvm::Value &V) {
  if (!m_dsa || !m_dsa->hasGraph(fn)) {
    return Region();
  }

  Graph &G = m_dsa->getGraph(fn);
  if (!G.hasCell(V)) {
    return Region();
  }

  const Cell &c = G.getCell(V);
  if (c.isNull()) {
    return Region();
  }

  RegionInfo r_info = SeaDsaToRegion(c, m_dl,
				     m_params.disambiguate_unknown,
				     m_params.disambiguate_ptr_cast,
				     m_params.disambiguate_external);
  return mkRegion(c, r_info);
}

Region SeaDsaHeapAbstractionImpl::getRegion(const llvm::Function &fn,
                                            const llvm::Value &V,
                                            unsigned offset,
                                            const Type &AccessedType) {

  if (!m_dsa || !m_dsa->hasGraph(fn)) {
    return Region();
  }

  Graph &G = m_dsa->getGraph(fn);
  if (!G.hasCell(V)) {
    return Region();
  }

  if (Node *n = G.getCell(V).getNode()) {
    if (n->hasAccessedType(offset)) {
      Cell c(n, offset);
      RegionInfo r_info =
          SeaDsaToRegion(c, m_dl,
			 m_params.disambiguate_unknown,
                         m_params.disambiguate_ptr_cast,
			 m_params.disambiguate_external);
      return mkRegion(c, r_info);
    }
  }
  return Region();
}

const llvm::Value *SeaDsaHeapAbstractionImpl::getSingleton(
    HeapAbstraction::RegionId region) const {
  auto const it = m_rev_node_ids.find(region);
  if (it == m_rev_node_ids.end())
    return nullptr;

  const Node *n = it->second;
  if (!n)
    return nullptr;
  if (const Value *v = n->getUniqueScalar()) {
    if (const GlobalVariable *gv = dyn_cast<const GlobalVariable>(v)) {
      seadsa_heap_abs_impl::isIntegerOrBool is_typed;
      if (is_typed(gv->getType()->getPointerElementType()))
        return v;
    }
  }
  return nullptr;
}

// Return v1 \ v2 by keeping the same ordering in v1.
// Precondition: v1 can have duplicates but v2 cannot.
//
// e.g., [1,3,3,4] \ [1,3] = [3,4]
// e.g., [1,3,3,3,4,5] \ [1,3,3] = [3,4,5]
static HeapAbstraction::RegionVec
stable_difference(HeapAbstraction::RegionVec &v1,
                  HeapAbstraction::RegionVec &v2) {
  // v1 and v2 can be modified because we pass copies

  std::sort(v2.begin(), v2.end());
  HeapAbstraction::RegionVec out;
  out.reserve(v1.size());
  for (auto rgn : v1) {
    auto lower = std::lower_bound(v2.begin(), v2.end(), rgn);
    if (lower != v2.end() && rgn == *lower) {
      // found
      //
      // remove from v2 so we handle correctly duplicates
      // FIXME: do not use erase, it's expensive
      v2.erase(lower);
    } else if (lower == v2.end() || rgn < *lower) {
      // not found
      out.push_back(rgn);
    }
  }
  return out;
}

template <class Map, class MapKey>
static HeapAbstraction::RegionVec lookup(const Map &m, const MapKey func) {
  auto it = m.find(func);
  if (it != m.end()) {
    return it->second;
  } else {
    return HeapAbstraction::RegionVec();
  }
}

HeapAbstraction::RegionVec
SeaDsaHeapAbstractionImpl::getOnlyReadRegions(const llvm::Function &fn) const {
  HeapAbstraction::RegionVec v1 = lookup(m_func_accessed, &fn);
  HeapAbstraction::RegionVec v2 = lookup(m_func_mods, &fn);
  return stable_difference(v1, v2);
}

HeapAbstraction::RegionVec
SeaDsaHeapAbstractionImpl::getModifiedRegions(const llvm::Function &fn) const {
  return lookup(m_func_mods, &fn);
}

HeapAbstraction::RegionVec
SeaDsaHeapAbstractionImpl::getNewRegions(const llvm::Function &fn) const {
  return lookup(m_func_news, &fn);
}

HeapAbstraction::RegionVec
SeaDsaHeapAbstractionImpl::getOnlyReadRegions(const llvm::CallInst &I) const {
  HeapAbstraction::RegionVec v1 = lookup(m_callsite_accessed, &I);
  HeapAbstraction::RegionVec v2 = lookup(m_callsite_mods, &I);
  return stable_difference(v1, v2);
}

HeapAbstraction::RegionVec
SeaDsaHeapAbstractionImpl::getModifiedRegions(const llvm::CallInst &I) const {
  return lookup(m_callsite_mods, &I);
}

HeapAbstraction::RegionVec
SeaDsaHeapAbstractionImpl::getNewRegions(const llvm::CallInst &I) const {
  return lookup(m_callsite_news, &I);
}

std::vector<HeapAbstraction::RegionVec>
SeaDsaHeapAbstractionImpl::getEquivClassRegions(
    const llvm::Function &fn) const {
  auto it = m_func_equiv_class_regions.find(&fn);
  if (it != m_func_equiv_class_regions.end()) {
    return it->second;
  } else {
    return std::vector<HeapAbstraction::RegionVec>();
  }
}

SeaDsaHeapAbstraction::SeaDsaHeapAbstraction(
    const llvm::Module &M, llvm::CallGraph &cg,
    const llvm::TargetLibraryInfoWrapperPass &tli,
    const seadsa::AllocWrapInfo &awi, const seadsa::DsaLibFuncInfo &sgi,
    SeaDsaHeapAbstractionParams params) {
  m_impl = std::make_unique<SeaDsaHeapAbstractionImpl>(
      M, cg, tli, awi, sgi, params);
}

SeaDsaHeapAbstraction::SeaDsaHeapAbstraction(const llvm::Module &M,
                                             seadsa::GlobalAnalysis &dsa,
					     SeaDsaHeapAbstractionParams params) {
  m_impl = std::make_unique<SeaDsaHeapAbstractionImpl>(M, dsa, params);
}

SeaDsaHeapAbstraction::~SeaDsaHeapAbstraction() {}

seadsa::GlobalAnalysis *SeaDsaHeapAbstraction::getSeaDsa() {
  return m_impl->getSeaDsa();
}

const seadsa::GlobalAnalysis *SeaDsaHeapAbstraction::getSeaDsa() const {
  return m_impl->getSeaDsa();
}

Region SeaDsaHeapAbstraction::getRegion(const llvm::Function &F,
                                        const llvm::Value &V) {
  return m_impl->getRegion(F, V);
}

Region SeaDsaHeapAbstraction::getRegion(const llvm::Function &F,
                                        const llvm::Value &V, unsigned offset,
                                        const llvm::Type &accessedType) {
  return m_impl->getRegion(F, V, offset, accessedType);
}

HeapAbstraction::RegionVec
SeaDsaHeapAbstraction::getOnlyReadRegions(const llvm::Function &F) const {
  return m_impl->getOnlyReadRegions(F);
}

HeapAbstraction::RegionVec
SeaDsaHeapAbstraction::getModifiedRegions(const llvm::Function &F) const {
  return m_impl->getModifiedRegions(F);
}

HeapAbstraction::RegionVec
SeaDsaHeapAbstraction::getNewRegions(const llvm::Function &F) const {
  return m_impl->getNewRegions(F);
}

HeapAbstraction::RegionVec
SeaDsaHeapAbstraction::getOnlyReadRegions(const llvm::CallInst &I) const {
  return m_impl->getOnlyReadRegions(I);
}

HeapAbstraction::RegionVec
SeaDsaHeapAbstraction::getModifiedRegions(const llvm::CallInst &I) const {
  return m_impl->getModifiedRegions(I);
}

HeapAbstraction::RegionVec
SeaDsaHeapAbstraction::getNewRegions(const llvm::CallInst &I) const {
  return m_impl->getNewRegions(I);
}

std::vector<HeapAbstraction::RegionVec>
SeaDsaHeapAbstraction::getEquivClassRegions(const llvm::Function &F) const {
  return m_impl->getEquivClassRegions(F);
}

} // namespace clam
