package protocol

import (
	"github.com/klauspost/compress/zstd"
	"github.com/yuanyuanxiang/SimpleRemoter/server/go/connection"
)

// Codec handles encoding/decoding and compression
type Codec struct {
	encoder *zstd.Encoder
	decoder *zstd.Decoder
}

// NewCodec creates a new codec
func NewCodec() *Codec {
	encoder, err := zstd.NewWriter(nil, zstd.WithEncoderLevel(zstd.SpeedDefault))
	if err != nil {
		panic("failed to create zstd encoder: " + err.Error())
	}
	decoder, err := zstd.NewReader(nil)
	if err != nil {
		panic("failed to create zstd decoder: " + err.Error())
	}

	return &Codec{
		encoder: encoder,
		decoder: decoder,
	}
}

// Compress compresses data using the appropriate method
func (c *Codec) Compress(ctx *connection.Context, data []byte) ([]byte, error) {
	switch ctx.CompressMethod {
	case connection.CompressNone:
		return data, nil
	case connection.CompressZstd:
		return c.encoder.EncodeAll(data, nil), nil
	default:
		// Default to zstd
		return c.encoder.EncodeAll(data, nil), nil
	}
}

// Decompress decompresses data using the appropriate method
func (c *Codec) Decompress(ctx *connection.Context, data []byte, origLen uint32) ([]byte, error) {
	switch ctx.CompressMethod {
	case connection.CompressNone:
		// No compression, return as-is
		result := make([]byte, len(data))
		copy(result, data)
		return result, nil
	case connection.CompressZstd:
		result := make([]byte, 0, origLen)
		return c.decoder.DecodeAll(data, result)
	default:
		// Try zstd by default
		result := make([]byte, 0, origLen)
		return c.decoder.DecodeAll(data, result)
	}
}

// Encode encodes data after compression (before sending) - Encoder2
func (c *Codec) Encode(ctx *connection.Context, data []byte) {
	// This is Encoder2 - applied after compression
	switch ctx.FlagType {
	case connection.FlagHello, connection.FlagHell:
		// XOREncoder16 - needs param from header
		// For now, skip encoding on send since we need the header params
	case connection.FlagFuck:
		// No encoding after compression for FUCK
	}
}

// Decode decodes data before decompression (after receiving) - Encoder2
func (c *Codec) Decode(ctx *connection.Context, data []byte) {
	// This is Encoder2 - applied before decompression
	// XOREncoder16 for HELL/HELLO protocols
	if ctx.FlagType == connection.FlagHell || ctx.FlagType == connection.FlagHello {
		// Get k1, k2 from stored header params
		if len(ctx.HeaderParams) >= 8 {
			k1 := ctx.HeaderParams[6]
			k2 := ctx.HeaderParams[7]
			if k1 != 0 || k2 != 0 {
				xorDecoder16(data, k1, k2)
			}
		}
	}
}

// EncodeData encodes data before compression - Encoder
func (c *Codec) EncodeData(ctx *connection.Context, data []byte) {
	// This is Encoder - applied before compression
	// Default encoder does nothing
}

// DecodeData decodes data after decompression - Encoder
func (c *Codec) DecodeData(ctx *connection.Context, data []byte) {
	// This is Encoder - applied after decompression
	// Default encoder does nothing
}

// xorDecoder16 implements XOREncoder16.decrypt_internal
func xorDecoder16(data []byte, k1, k2 byte) {
	if len(data) == 0 {
		return
	}

	key := (uint16(k1) << 8) | uint16(k2)
	dataLen := len(data)

	// Reverse two rounds of pseudo-random swaps
	for round := 1; round >= 0; round-- {
		for i := dataLen - 1; i >= 0; i-- {
			j := int(pseudoRandom(key, i+round*100)) % dataLen
			data[i], data[j] = data[j], data[i]
		}
	}

	// XOR decode
	for i := 0; i < dataLen; i++ {
		data[i] ^= (k1 + byte(i*13)) ^ (k2 ^ byte(i<<1))
	}
}

// xorEncoder16 implements XOREncoder16.encrypt_internal
func xorEncoder16(data []byte, k1, k2 byte) {
	if len(data) == 0 {
		return
	}

	key := (uint16(k1) << 8) | uint16(k2)
	dataLen := len(data)

	// XOR encode
	for i := 0; i < dataLen; i++ {
		data[i] ^= (k1 + byte(i*13)) ^ (k2 ^ byte(i<<1))
	}

	// Two rounds of pseudo-random swaps
	for round := 0; round < 2; round++ {
		for i := 0; i < dataLen; i++ {
			j := int(pseudoRandom(key, i+round*100)) % dataLen
			data[i], data[j] = data[j], data[i]
		}
	}
}

// pseudoRandom matches the C++ pseudo_random function
func pseudoRandom(seed uint16, index int) uint16 {
	return ((seed ^ uint16(index*251+97)) * 733) ^ (seed >> 3)
}

// Close releases resources
func (c *Codec) Close() {
	if c.encoder != nil {
		c.encoder.Close()
	}
	if c.decoder != nil {
		c.decoder.Close()
	}
}
