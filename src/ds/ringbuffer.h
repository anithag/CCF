// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the Apache 2.0 License.
#pragma once

#include "ringbuffer_types.h"

#include <atomic>
#include <cstring>
#include <functional>

extern "C" {
#include "verified/Ring_Reader_Writer_Misc.h"
}

// Ideally this would be _mm_pause or similar, but finding cross-platform
// headers that expose this neatly through OE (ie - non-standard std libs) is
// awkward. Instead we resort to copying OE, and implementing this directly
// ourselves.
#define CCF_PAUSE() asm volatile("pause")

// This file implements a Multiple-Producer Single-Consumer ringbuffer.

// A single Reader instance owns an underlying memory buffer, and a single
// thread should process message written to it. Any number of other threads and
// Writers may write to it, and the messages will be distinct, correct, and
// ordered.

// A Circuit wraps a pair of ringbuffers to allow 2-way communication - messages
// are written to the inbound buffer, processed inside an enclave, and responses
// written back to the outbound.

namespace ringbuffer
{
  using Handler = std::function<void(Message, const uint8_t*, size_t)>;

  // Align by cacheline to avoid false sharing
  static constexpr size_t CACHELINE_SIZE = 64;

  // High bit of message size is used to indicate a pending message
  static constexpr uint32_t pending_write_flag = 1 << 31;
  static constexpr uint32_t length_mask = ~pending_write_flag;

  struct alignas(CACHELINE_SIZE) Var
  {
    std::atomic<size_t> head_cache;
    std::atomic<size_t> tail;
    alignas(CACHELINE_SIZE) std::atomic<size_t> head;
  };

  struct Const
  {
    enum : Message
    {
      msg_max = std::numeric_limits<Message>::max() - 1,
      msg_min = 1,
      msg_none = 0,
      msg_pad = std::numeric_limits<Message>::max()
    };

    static constexpr bool is_power_of_2(size_t n)
    {
      return n && ((n & (~n + 1)) == n);
    }

    static constexpr size_t header_size()
    {
      // The header is a 32 bit length and a 32 bit message ID.
      return sizeof(int32_t) + sizeof(uint32_t);
    }

    static constexpr size_t align_size(size_t n)
    {
      // Make sure the header is aligned in memory.
      return (n + (header_size() - 1)) & ~(header_size() - 1);
    }

    static constexpr size_t entry_size(size_t n)
    {
      return Const::align_size(n + header_size());
    }

    static constexpr size_t max_size()
    {
      // The length of a message plus its header must be encodable in the
      // header. High bit of lengths indicate pending writes.
      return std::numeric_limits<int32_t>::max() - header_size();
    }

    static constexpr size_t max_reservation_size(size_t buffer_size)
    {
      // This guarantees that in an empty buffer, we can always make this
      // reservation in a single contiguous region (either before or after the
      // current tail). If we allow larger reservations then we may need to
      // artificially advance the tail (writing padding then clearing it) to
      // create a sufficiently large region.
      return buffer_size / 2;
    }

    uint8_t* const buffer;
    const size_t size;

    Const(uint8_t* const buffer, size_t size) : buffer(buffer), size(size)
    {
      if (!is_power_of_2(size))
        throw std::logic_error("Buffer size must be a power of 2");
    }
  };

 class Reader {
    friend class Writer;
    Ring_ringstruct__uint8_t r;
  public:

    Reader(const size_t size)
     {
       // init ringbuffer
       r = Reader_init(size);
     }
    
    size_t read(size_t limit, Handler f)
    {
      void (**myf)(unsigned int, const unsigned char*, unsigned long) = f.target<void (*)(unsigned int, const unsigned char *, unsigned long)>();
      if (myf) {
	return Reader_read(this->r, *myf); 
      } else
	// Handler could be capturing some variables
	return 0;
    }
     
  };

  class Writer : public AbstractWriter {

    // pointer to reader's ringbuffer
    Ring_ringstruct__uint8_t r;
  protected:
    virtual WriteMarker write_bytes(const WriteMarker& marker, const uint8_t* bytes, size_t size) override
    {
      return 0;
    }

  public:

    Writer(const Reader& reader) : r(reader.r) {}

    Writer(const Writer& that) : r(that.r) {}
    virtual ~Writer() {}

    //fix later
    virtual std::optional<size_t> prepare(Message m,
      size_t size,
      bool wait = true,
      size_t* identifier = nullptr) override
    {
      return 0;
    }
    
    virtual void finish(const WriteMarker& marker) override {}

   void write32(size_t index, uint32_t value)
    {
      Writer_write(this->r, value); 
    }
  };
 
  // This is entirely non-virtual so can be safely passed to the enclave
  class Circuit
  {
  private:
    ringbuffer::Reader from_outside;
    ringbuffer::Reader from_inside;

  public:
    Circuit(size_t size) : from_outside(size), from_inside(size) {}

    ringbuffer::Reader& read_from_outside()
    {
      return from_outside;
    }

    ringbuffer::Reader& read_from_inside()
    {
      return from_inside;
    }

    ringbuffer::Writer write_to_outside()
    {
      return ringbuffer::Writer(from_inside);
    }

    ringbuffer::Writer write_to_inside()
    {
      return ringbuffer::Writer(from_outside);
    }
  };

  class WriterFactory : public AbstractWriterFactory
  {
    ringbuffer::Circuit& raw_circuit;

  public:
    WriterFactory(ringbuffer::Circuit& c) : raw_circuit(c) {}

    std::shared_ptr<ringbuffer::AbstractWriter> create_writer_to_outside()
      override
    {
      return std::make_shared<Writer>(raw_circuit.read_from_inside());
    }

    std::shared_ptr<ringbuffer::AbstractWriter> create_writer_to_inside()
      override
    {
      return std::make_shared<Writer>(raw_circuit.read_from_outside());
    }
  };
}
