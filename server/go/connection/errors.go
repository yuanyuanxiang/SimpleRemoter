package connection

import "errors"

var (
	// ErrConnectionClosed indicates the connection is closed
	ErrConnectionClosed = errors.New("connection closed")

	// ErrServerClosed indicates the server is shut down
	ErrServerClosed = errors.New("server closed")

	// ErrMaxConnections indicates max connections reached
	ErrMaxConnections = errors.New("max connections reached")

	// ErrInvalidPacket indicates an invalid packet
	ErrInvalidPacket = errors.New("invalid packet")

	// ErrUnsupportedProtocol indicates unsupported protocol
	ErrUnsupportedProtocol = errors.New("unsupported protocol")

	// ErrDecompressFailed indicates decompression failure
	ErrDecompressFailed = errors.New("decompression failed")
)
