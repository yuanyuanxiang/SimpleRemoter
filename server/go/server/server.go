package server

import (
	"context"
	"io"
	"net"
	"sync"
	"sync/atomic"
	"time"

	"github.com/yuanyuanxiang/SimpleRemoter/server/go/connection"
	"github.com/yuanyuanxiang/SimpleRemoter/server/go/logger"
	"github.com/yuanyuanxiang/SimpleRemoter/server/go/protocol"
)

// Config holds server configuration
type Config struct {
	Port            int
	MaxConnections  int
	ReadBufferSize  int
	WriteBufferSize int
	KeepAliveTime   time.Duration
	ReadTimeout     time.Duration
	WriteTimeout    time.Duration
}

// DefaultConfig returns default configuration
func DefaultConfig() Config {
	return Config{
		Port:            6543,
		MaxConnections:  9999,
		ReadBufferSize:  8192,
		WriteBufferSize: 8192,
		KeepAliveTime:   time.Minute * 5,
		ReadTimeout:     time.Minute * 2,
		WriteTimeout:    time.Second * 30,
	}
}

// Handler defines the interface for handling client events
type Handler interface {
	OnConnect(ctx *connection.Context)
	OnDisconnect(ctx *connection.Context)
	OnReceive(ctx *connection.Context, data []byte)
}

// Server is the TCP server
type Server struct {
	config   Config
	listener net.Listener
	manager  *connection.Manager
	handler  Handler
	parser   *protocol.Parser

	running atomic.Bool
	wg      sync.WaitGroup
	ctx     context.Context
	cancel  context.CancelFunc

	// Logger
	logger *logger.Logger
}

// New creates a new server
func New(config Config) *Server {
	ctx, cancel := context.WithCancel(context.Background())

	s := &Server{
		config:  config,
		manager: connection.NewManager(config.MaxConnections),
		parser:  protocol.NewParser(),
		ctx:     ctx,
		cancel:  cancel,
		logger:  logger.New(logger.DefaultConfig()).WithPrefix("Server"),
	}

	return s
}

// SetHandler sets the event handler
func (s *Server) SetHandler(h Handler) {
	s.handler = h
	s.manager.SetCallbacks(
		func(ctx *connection.Context) {
			if s.handler != nil {
				s.handler.OnConnect(ctx)
			}
		},
		func(ctx *connection.Context) {
			if s.handler != nil {
				s.handler.OnDisconnect(ctx)
			}
		},
		func(ctx *connection.Context, data []byte) {
			if s.handler != nil {
				s.handler.OnReceive(ctx, data)
			}
		},
	)
}

// SetLogger sets the logger
func (s *Server) SetLogger(l *logger.Logger) {
	s.logger = l
}

// Start starts the server
func (s *Server) Start() error {
	addr := net.JoinHostPort("0.0.0.0", itoa(s.config.Port))
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}

	s.listener = listener
	s.running.Store(true)

	s.logger.Info("Server started on port %d", s.config.Port)

	s.wg.Add(1)
	go s.acceptLoop()

	return nil
}

func itoa(i int) string {
	if i == 0 {
		return "0"
	}
	var b [20]byte
	pos := len(b)
	neg := i < 0
	if neg {
		// Handle math.MinInt safely by using unsigned
		u := uint(-i)
		for u > 0 {
			pos--
			b[pos] = byte('0' + u%10)
			u /= 10
		}
		pos--
		b[pos] = '-'
	} else {
		for i > 0 {
			pos--
			b[pos] = byte('0' + i%10)
			i /= 10
		}
	}
	return string(b[pos:])
}

// Stop stops the server gracefully
func (s *Server) Stop() {
	if !s.running.Swap(false) {
		return
	}

	s.cancel()

	if s.listener != nil {
		_ = s.listener.Close()
	}

	// Close all connections
	s.manager.CloseAll()

	// Close parser resources
	if s.parser != nil {
		s.parser.Close()
	}

	s.wg.Wait()
	s.logger.Info("Server stopped")
}

// acceptLoop accepts incoming connections
func (s *Server) acceptLoop() {
	defer s.wg.Done()

	for s.running.Load() {
		conn, err := s.listener.Accept()
		if err != nil {
			if s.running.Load() {
				s.logger.Error("Accept error: %v", err)
			}
			continue
		}

		// Check connection limit before spawning goroutine
		if s.manager.Count() >= s.config.MaxConnections {
			s.logger.Warn("Max connections reached, rejecting new connection from %s", conn.RemoteAddr())
			_ = conn.Close()
			continue
		}

		// Handle each connection in its own goroutine
		go s.handleConnection(conn)
	}
}

// handleConnection handles a single connection
func (s *Server) handleConnection(conn net.Conn) {
	// Create context
	ctx := connection.NewContext(conn, nil)

	// Add to manager
	if err := s.manager.Add(ctx); err != nil {
		s.logger.Warn("Failed to add connection: %v", err)
		_ = conn.Close()
		return
	}

	defer func() {
		s.manager.Remove(ctx)
		_ = ctx.Close()
	}()

	// Read loop
	buf := make([]byte, s.config.ReadBufferSize)
	for !ctx.IsClosed() && s.running.Load() {
		// Set read deadline
		if s.config.ReadTimeout > 0 {
			_ = conn.SetReadDeadline(time.Now().Add(s.config.ReadTimeout))
		}

		n, err := conn.Read(buf)
		if err != nil {
			if err != io.EOF && s.running.Load() {
				if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
					// Timeout - check keepalive
					if s.config.KeepAliveTime > 0 && ctx.TimeSinceLastActive() > s.config.KeepAliveTime {
						s.logger.Info("Connection %d timed out", ctx.ID)
						break
					}
					continue
				}
			}
			break
		}

		if n > 0 {
			ctx.UpdateLastActive()

			// Write to input buffer
			_, _ = ctx.InBuffer.Write(buf[:n])

			// Process received data
			s.processData(ctx)
		}
	}
}

// processData processes received data and calls handler
func (s *Server) processData(ctx *connection.Context) {
	for ctx.InBuffer.Len() > 0 {
		// Try to parse a complete packet
		data, err := s.parser.Parse(ctx)
		if err != nil {
			if err == protocol.ErrNeedMore {
				return
			}
			// Log more details for unsupported protocol errors
			if err == protocol.ErrUnsupported {
				// Get first 32 bytes for debugging
				peekLen := 32
				if ctx.InBuffer.Len() < peekLen {
					peekLen = ctx.InBuffer.Len()
				}
				rawData := ctx.InBuffer.Peek(peekLen)
				// Sanitize for safe logging (replace non-printable chars)
				ascii := make([]byte, len(rawData))
				for i, b := range rawData {
					if b >= 32 && b < 127 {
						ascii[i] = b
					} else {
						ascii[i] = '.'
					}
				}
				s.logger.Warn("Unsupported protocol from ip=%s conn=%d raw=%x ascii=%s",
					ctx.GetPeerIP(), ctx.ID, rawData, string(ascii))
			} else {
				s.logger.Error("Parse error for connection %d: %v", ctx.ID, err)
			}
			_ = ctx.Close()
			return
		}

		if data != nil {
			// Call handler
			s.manager.OnReceive(ctx, data)
		}
	}
}

// Send sends data to a specific connection
func (s *Server) Send(ctx *connection.Context, data []byte) error {
	if ctx == nil || ctx.IsClosed() {
		return connection.ErrConnectionClosed
	}

	// Encode and compress data
	encoded, err := s.parser.Encode(ctx, data)
	if err != nil {
		return err
	}

	return ctx.Send(encoded)
}

// Broadcast sends data to all connections
func (s *Server) Broadcast(data []byte) {
	s.manager.Range(func(ctx *connection.Context) bool {
		_ = s.Send(ctx, data)
		return true
	})
}

// GetConnection returns a connection by ID
func (s *Server) GetConnection(id uint64) *connection.Context {
	return s.manager.Get(id)
}

// ConnectionCount returns the current connection count
func (s *Server) ConnectionCount() int {
	return s.manager.Count()
}

// Port returns the server port
func (s *Server) Port() int {
	return s.config.Port
}

// UpdateMaxConnections updates the max connections limit
func (s *Server) UpdateMaxConnections(max int) {
	s.manager.UpdateMaxConnections(max)
}
