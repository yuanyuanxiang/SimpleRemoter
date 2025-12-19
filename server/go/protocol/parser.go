package protocol

import (
	"encoding/binary"
	"errors"

	"github.com/yuanyuanxiang/SimpleRemoter/server/go/connection"
)

// Errors
var (
	ErrNeedMore    = errors.New("need more data")
	ErrInvalidData = errors.New("invalid data")
	ErrUnsupported = errors.New("unsupported protocol")
	ErrDecompress  = errors.New("decompression failed")
)

// Parser handles protocol parsing
type Parser struct {
	codec *Codec
}

// NewParser creates a new parser
func NewParser() *Parser {
	return &Parser{
		codec: NewCodec(),
	}
}

// Close releases resources held by the parser
func (p *Parser) Close() {
	if p.codec != nil {
		p.codec.Close()
	}
}

// Parse attempts to parse a complete packet from the buffer
func (p *Parser) Parse(ctx *connection.Context) ([]byte, error) {
	buf := ctx.InBuffer

	// Need at least minimum bytes to determine protocol
	if buf.Len() < MinComLen {
		return nil, ErrNeedMore
	}

	// Check if header is already parsed
	if ctx.FlagType == connection.FlagUnknown {
		// Try to parse header
		if err := p.parseHeader(ctx); err != nil {
			return nil, err
		}
	}

	// Now parse the packet
	return p.parsePacket(ctx)
}

// parseHeader parses the protocol header with obfuscation handling
func (p *Parser) parseHeader(ctx *connection.Context) error {
	buf := ctx.InBuffer
	header := buf.Peek(MinComLen)
	if header == nil || len(header) < MinComLen {
		return ErrNeedMore
	}

	// Try to decode the header using all encryption methods
	flagType, encType, decrypted := CheckHead(header)

	if flagType == FlagUnknown {
		return ErrUnsupported
	}

	// Store decrypted header params for later use (for XOREncoder16)
	ctx.HeaderParams = make([]byte, MinComLen)
	if decrypted != nil {
		copy(ctx.HeaderParams, decrypted)
	} else {
		copy(ctx.HeaderParams, header)
	}

	// Map protocol FlagType to connection FlagType and set compression method
	switch flagType {
	case FlagHell:
		ctx.FlagType = connection.FlagHell
		ctx.FlagLen = FlagLength
		ctx.HeaderLen = ctx.FlagLen + 8
		ctx.CompressMethod = connection.CompressZstd // HELL uses ZSTD
	case FlagHello:
		ctx.FlagType = connection.FlagHello
		ctx.FlagLen = 8
		ctx.HeaderLen = ctx.FlagLen + 8
		ctx.CompressMethod = connection.CompressNone // HELLO uses no compression
	case FlagShine:
		ctx.FlagType = connection.FlagShine
		ctx.FlagLen = 5
		ctx.HeaderLen = ctx.FlagLen + 8
		ctx.CompressMethod = connection.CompressZstd // SHINE uses ZSTD
	case FlagFuck:
		ctx.FlagType = connection.FlagFuck
		ctx.FlagLen = 11
		ctx.HeaderLen = ctx.FlagLen + 8
		ctx.CompressMethod = connection.CompressZstd // FUCK uses ZSTD
	default:
		return ErrUnsupported
	}

	// Store encryption type for later use
	ctx.HeaderEncType = int(encType)

	return nil
}

// parsePacket parses a complete packet
func (p *Parser) parsePacket(ctx *connection.Context) ([]byte, error) {
	buf := ctx.InBuffer

	// Check if we have enough data for header
	if buf.Len() < ctx.HeaderLen {
		return nil, ErrNeedMore
	}

	// Peek the header to get total length
	headerData := buf.Peek(ctx.HeaderLen)
	if headerData == nil {
		return nil, ErrNeedMore
	}

	// Decrypt the header first
	decryptedHeader := make([]byte, len(headerData))
	copy(decryptedHeader, headerData)

	// Get the encryption key (usually at position 6)
	var key byte
	if len(headerData) > 6 {
		key = headerData[6] // Use original key before decryption
	}

	// Decrypt flag portion
	if ctx.HeaderEncType >= 0 && ctx.HeaderEncType < len(decryptMethods) {
		decryptMethods[ctx.HeaderEncType](decryptedHeader[:FlagCompLen], key)
	}

	// Read the total length field (after flag)
	totalLen := binary.LittleEndian.Uint32(decryptedHeader[ctx.FlagLen:])

	// Validate length
	if totalLen < uint32(ctx.HeaderLen) || totalLen > 10*1024*1024 {
		return nil, ErrInvalidData
	}

	// Check if we have the complete packet
	if buf.Len() < int(totalLen) {
		return nil, ErrNeedMore
	}

	// Read the complete packet
	packet := buf.Read(int(totalLen))
	if packet == nil {
		return nil, ErrInvalidData
	}

	// Decrypt header portion of packet
	if ctx.HeaderEncType >= 0 && ctx.HeaderEncType < len(decryptMethods) {
		decryptMethods[ctx.HeaderEncType](packet[:FlagCompLen], key)
	}

	// Update HeaderParams with this packet's header (for XOREncoder16 k1, k2)
	if len(packet) >= ctx.FlagLen {
		ctx.HeaderParams = make([]byte, ctx.HeaderLen)
		copy(ctx.HeaderParams, packet[:ctx.HeaderLen])
	}

	// Extract data after header
	dataStart := ctx.HeaderLen
	data := packet[dataStart:]

	// Get original length (before compression)
	var origLen uint32
	if ctx.FlagType != connection.FlagWinOS {
		origLen = binary.LittleEndian.Uint32(packet[ctx.FlagLen+4 : ctx.FlagLen+8])
	}

	// Decode (XOR, etc.) before decompression - this is Encoder2
	p.codec.Decode(ctx, data)

	// Decompress
	decompressed, err := p.codec.Decompress(ctx, data, origLen)
	if err != nil {
		return nil, err
	}

	// Decode after decompression - this is Encoder
	p.codec.DecodeData(ctx, decompressed)

	return decompressed, nil
}

// Encode encodes data for sending
func (p *Parser) Encode(ctx *connection.Context, data []byte) ([]byte, error) {
	// Encode before compression
	encoded := make([]byte, len(data))
	copy(encoded, data)
	p.codec.EncodeData(ctx, encoded)

	// Compress
	compressed, err := p.codec.Compress(ctx, encoded)
	if err != nil {
		return nil, err
	}

	// Build packet
	packet := p.buildPacket(ctx, compressed, uint32(len(data)))

	return packet, nil
}

// buildPacket builds a complete packet with header
func (p *Parser) buildPacket(ctx *connection.Context, data []byte, origLen uint32) []byte {
	totalLen := ctx.HeaderLen + len(data)
	packet := make([]byte, totalLen)

	// Write flag
	flag := p.getFlag(ctx)
	copy(packet[:ctx.FlagLen], flag)

	// Write total length
	binary.LittleEndian.PutUint32(packet[ctx.FlagLen:], uint32(totalLen))

	// Write original length
	binary.LittleEndian.PutUint32(packet[ctx.FlagLen+4:], origLen)

	// Write data
	copy(packet[ctx.HeaderLen:], data)

	// Encode after building
	p.codec.Encode(ctx, packet[ctx.HeaderLen:])

	return packet
}

// getFlag returns the protocol flag bytes
func (p *Parser) getFlag(ctx *connection.Context) []byte {
	switch ctx.FlagType {
	case connection.FlagHell:
		flag := make([]byte, FlagLength)
		copy(flag, []byte(MsgHeader))
		return flag
	case connection.FlagHello:
		flag := make([]byte, 8)
		copy(flag, []byte("Hello?"))
		return flag
	case connection.FlagShine:
		return []byte("Shine")
	case connection.FlagFuck:
		flag := make([]byte, 11)
		copy(flag, []byte("<<FUCK>>"))
		return flag
	default:
		return make([]byte, ctx.FlagLen)
	}
}
