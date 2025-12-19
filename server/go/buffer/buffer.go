package buffer

import (
	"encoding/binary"
	"sync"
)

// Buffer is a thread-safe dynamic buffer for network I/O
type Buffer struct {
	data   []byte
	mu     sync.RWMutex
	offset int // read offset for lazy compaction
}

// New creates a new buffer with optional initial capacity
func New(capacity ...int) *Buffer {
	cap := 4096
	if len(capacity) > 0 && capacity[0] > 0 {
		cap = capacity[0]
	}
	return &Buffer{
		data: make([]byte, 0, cap),
	}
}

// Write appends data to the buffer
func (b *Buffer) Write(p []byte) (int, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.data = append(b.data, p...)
	return len(p), nil
}

// WriteUint32 writes a uint32 in little-endian format
func (b *Buffer) WriteUint32(v uint32) {
	buf := make([]byte, 4)
	binary.LittleEndian.PutUint32(buf, v)
	b.Write(buf)
}

// Read reads and removes data from the buffer
func (b *Buffer) Read(n int) []byte {
	b.mu.Lock()
	defer b.mu.Unlock()

	available := len(b.data) - b.offset
	if n > available {
		n = available
	}
	if n <= 0 {
		return nil
	}

	result := make([]byte, n)
	copy(result, b.data[b.offset:b.offset+n])
	b.offset += n

	// Compact when offset is large enough
	if b.offset > len(b.data)/2 && b.offset > 1024 {
		b.compact()
	}

	return result
}

// Peek returns data without removing it
func (b *Buffer) Peek(n int) []byte {
	b.mu.RLock()
	defer b.mu.RUnlock()

	available := len(b.data) - b.offset
	if n > available {
		n = available
	}
	if n <= 0 {
		return nil
	}

	result := make([]byte, n)
	copy(result, b.data[b.offset:b.offset+n])
	return result
}

// PeekAt returns data at a specific offset without removing it
func (b *Buffer) PeekAt(offset, n int) []byte {
	b.mu.RLock()
	defer b.mu.RUnlock()

	start := b.offset + offset
	if start >= len(b.data) {
		return nil
	}

	end := start + n
	if end > len(b.data) {
		end = len(b.data)
	}

	result := make([]byte, end-start)
	copy(result, b.data[start:end])
	return result
}

// Skip removes n bytes from the beginning
func (b *Buffer) Skip(n int) int {
	b.mu.Lock()
	defer b.mu.Unlock()

	available := len(b.data) - b.offset
	if n > available {
		n = available
	}
	b.offset += n

	// Compact when offset is large enough
	if b.offset > len(b.data)/2 && b.offset > 1024 {
		b.compact()
	}

	return n
}

// Len returns the length of unread data
func (b *Buffer) Len() int {
	b.mu.RLock()
	defer b.mu.RUnlock()
	return len(b.data) - b.offset
}

// Bytes returns all unread data without removing it
func (b *Buffer) Bytes() []byte {
	b.mu.RLock()
	defer b.mu.RUnlock()

	n := len(b.data) - b.offset
	if n <= 0 {
		return nil
	}

	result := make([]byte, n)
	copy(result, b.data[b.offset:])
	return result
}

// Clear removes all data from the buffer
func (b *Buffer) Clear() {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.data = b.data[:0]
	b.offset = 0
}

// compact moves remaining data to the beginning
func (b *Buffer) compact() {
	if b.offset > 0 {
		remaining := len(b.data) - b.offset
		copy(b.data[:remaining], b.data[b.offset:])
		b.data = b.data[:remaining]
		b.offset = 0
	}
}

// GetByte returns a single byte at offset
func (b *Buffer) GetByte(offset int) byte {
	b.mu.RLock()
	defer b.mu.RUnlock()

	idx := b.offset + offset
	if idx >= len(b.data) {
		return 0
	}
	return b.data[idx]
}

// GetUint32 returns a uint32 at offset in little-endian format
func (b *Buffer) GetUint32(offset int) uint32 {
	b.mu.RLock()
	defer b.mu.RUnlock()

	idx := b.offset + offset
	if idx+4 > len(b.data) {
		return 0
	}
	return binary.LittleEndian.Uint32(b.data[idx : idx+4])
}
