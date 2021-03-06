// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_WRITE_BARRIER_INL_H_
#define V8_HEAP_HEAP_WRITE_BARRIER_INL_H_

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap here!

#include "src/heap/heap-write-barrier.h"

#include "src/globals.h"
#include "src/objects-inl.h"
#include "src/objects/maybe-object-inl.h"

namespace v8 {
namespace internal {

// Do not use these internal details anywhere outside of this file. These
// internals are only intended to shortcut write barrier checks.
namespace heap_internals {

struct MemoryChunk {
  static constexpr uintptr_t kFlagsOffset = sizeof(size_t);
  static constexpr uintptr_t kMarkingBit = uintptr_t{1} << 18;
  static constexpr uintptr_t kFromSpaceBit = uintptr_t{1} << 3;
  static constexpr uintptr_t kToSpaceBit = uintptr_t{1} << 4;

  V8_INLINE static heap_internals::MemoryChunk* FromHeapObject(
      HeapObject* object) {
    return reinterpret_cast<MemoryChunk*>(reinterpret_cast<Address>(object) &
                                          ~kPageAlignmentMask);
  }

  V8_INLINE bool IsMarking() const { return GetFlags() & kMarkingBit; }

  V8_INLINE bool InNewSpace() const {
    constexpr uintptr_t kNewSpaceMask = kFromSpaceBit | kToSpaceBit;
    return GetFlags() & kNewSpaceMask;
  }

  V8_INLINE uintptr_t GetFlags() const {
    return *reinterpret_cast<const uintptr_t*>(
        reinterpret_cast<const uint8_t*>(this) + kFlagsOffset);
  }
};

inline void GenerationalBarrierInternal(HeapObject* object, Address slot,
                                        HeapObject* value) {
  DCHECK(Heap::PageFlagsAreConsistent(object));
  heap_internals::MemoryChunk* value_chunk =
      heap_internals::MemoryChunk::FromHeapObject(value);
  heap_internals::MemoryChunk* object_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);

  if (!value_chunk->InNewSpace() || object_chunk->InNewSpace()) return;

  Heap::GenerationalBarrierSlow(object, slot, value);
}

inline void MarkingBarrierInternal(HeapObject* object, Address slot,
                                   HeapObject* value) {
  DCHECK(Heap::PageFlagsAreConsistent(object));
  heap_internals::MemoryChunk* value_chunk =
      heap_internals::MemoryChunk::FromHeapObject(value);

  if (!value_chunk->IsMarking()) return;

  Heap::MarkingBarrierSlow(object, slot, value);
}

}  // namespace heap_internals

inline void GenerationalBarrier(HeapObject* object, Object** slot,
                                Object* value) {
  DCHECK(!HasWeakHeapObjectTag(*slot));
  DCHECK(!HasWeakHeapObjectTag(value));
  if (!value->IsHeapObject()) return;
  heap_internals::GenerationalBarrierInternal(
      object, reinterpret_cast<Address>(slot), HeapObject::cast(value));
}

inline void GenerationalBarrier(HeapObject* object, MaybeObject** slot,
                                MaybeObject* value) {
  HeapObject* value_heap_object;
  if (!value->ToStrongOrWeakHeapObject(&value_heap_object)) return;
  heap_internals::GenerationalBarrierInternal(
      object, reinterpret_cast<Address>(slot), value_heap_object);
}

inline void MarkingBarrier(HeapObject* object, Object** slot, Object* value) {
  DCHECK_IMPLIES(slot != nullptr, !HasWeakHeapObjectTag(*slot));
  DCHECK(!HasWeakHeapObjectTag(value));
  if (!value->IsHeapObject()) return;
  heap_internals::MarkingBarrierInternal(
      object, reinterpret_cast<Address>(slot), HeapObject::cast(value));
}

inline void MarkingBarrier(HeapObject* object, MaybeObject** slot,
                           MaybeObject* value) {
  HeapObject* value_heap_object;
  if (!value->ToStrongOrWeakHeapObject(&value_heap_object)) return;
  heap_internals::MarkingBarrierInternal(
      object, reinterpret_cast<Address>(slot), value_heap_object);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_WRITE_BARRIER_INL_H_
