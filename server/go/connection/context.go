package connection

import (
	"fmt"
	"net"
	"sync"
	"sync/atomic"
	"time"

	"github.com/yuanyuanxiang/SimpleRemoter/server/go/buffer"
)

// ClientInfo stores client metadata
type ClientInfo struct {
	ClientID     string // Client ID from login (MasterID)
	IP           string
	ComputerName string
	OS           string
	CPU          string
	HasCamera    bool
	Version      string
	InstallTime  string
	LoginTime    time.Time
	ClientType   string
	FilePath     string
	GroupName    string
}

// Context represents a client connection context
type Context struct {
	ID         uint64
	Conn       net.Conn
	RemoteAddr string

	// Buffers
	InBuffer  *buffer.Buffer // Received compressed data
	OutBuffer *buffer.Buffer // Decompressed data for processing

	// Client info
	Info         ClientInfo
	IsLoggedIn   atomic.Bool
	IsAuthorized atomic.Bool // Whether client is authorized via heartbeat

	// Connection state
	OnlineTime   time.Time
	lastActiveNs atomic.Int64 // Unix nanoseconds for thread-safe access

	// Protocol state
	CompressMethod int
	FlagType       FlagType
	HeaderLen      int
	FlagLen        int
	HeaderEncType  int    // Header encryption type (0-7)
	HeaderParams   []byte // Header parameters for decoding (flag bytes)

	// User data - for storing dialog/handler references
	UserData interface{}

	// Internal
	mu       sync.RWMutex
	closed   atomic.Bool
	sendLock sync.Mutex
	server   *Manager
}

// FlagType represents the protocol flag type
type FlagType int

const (
	FlagUnknown FlagType = iota
	FlagShine
	FlagFuck
	FlagHello
	FlagHell
	FlagWinOS
)

// Compression methods
const (
	CompressUnknown = -2
	CompressZlib    = -1
	CompressZstd    = 0
	CompressNone    = 1
)

// NewContext creates a new connection context
func NewContext(conn net.Conn, mgr *Manager) *Context {
	now := time.Now()
	ctx := &Context{
		Conn:           conn,
		RemoteAddr:     conn.RemoteAddr().String(),
		InBuffer:       buffer.New(8192),
		OutBuffer:      buffer.New(8192),
		OnlineTime:     now,
		CompressMethod: CompressZstd,
		FlagType:       FlagUnknown,
		server:         mgr,
	}
	ctx.lastActiveNs.Store(now.UnixNano())
	return ctx
}

// Send sends data to the client (thread-safe)
func (c *Context) Send(data []byte) error {
	if c.closed.Load() {
		return ErrConnectionClosed
	}

	c.sendLock.Lock()
	defer c.sendLock.Unlock()

	_, err := c.Conn.Write(data)
	if err != nil {
		return err
	}
	c.UpdateLastActive()
	return nil
}

// UpdateLastActive updates the last active time (thread-safe)
func (c *Context) UpdateLastActive() {
	c.lastActiveNs.Store(time.Now().UnixNano())
}

// LastActive returns the last active time (thread-safe)
func (c *Context) LastActive() time.Time {
	return time.Unix(0, c.lastActiveNs.Load())
}

// TimeSinceLastActive returns duration since last activity (thread-safe)
func (c *Context) TimeSinceLastActive() time.Duration {
	return time.Since(c.LastActive())
}

// Close closes the connection
func (c *Context) Close() error {
	if c.closed.Swap(true) {
		return nil // Already closed
	}
	return c.Conn.Close()
}

// IsClosed returns whether the connection is closed
func (c *Context) IsClosed() bool {
	return c.closed.Load()
}

// GetPeerIP returns the peer IP address
func (c *Context) GetPeerIP() string {
	if host, _, err := net.SplitHostPort(c.RemoteAddr); err == nil {
		return host
	}
	return c.RemoteAddr
}

// AliveTime returns how long the connection has been alive
func (c *Context) AliveTime() time.Duration {
	return time.Since(c.OnlineTime)
}

// SetInfo sets the client info
func (c *Context) SetInfo(info ClientInfo) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.Info = info
}

// GetInfo returns the client info
func (c *Context) GetInfo() ClientInfo {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.Info
}

// SetUserData stores user-defined data
func (c *Context) SetUserData(data interface{}) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.UserData = data
}

// GetUserData retrieves user-defined data
func (c *Context) GetUserData() interface{} {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.UserData
}

// GetClientID returns the client ID for logging
// If ClientID is set (from login), returns it; otherwise returns connection ID as fallback
func (c *Context) GetClientID() string {
	c.mu.RLock()
	defer c.mu.RUnlock()
	if c.Info.ClientID != "" {
		return c.Info.ClientID
	}
	return fmt.Sprintf("conn-%d", c.ID)
}
