package connection

import (
	"sync"
	"sync/atomic"
)

// Manager manages all client connections
type Manager struct {
	connections sync.Map // map[uint64]*Context
	count       atomic.Int64
	maxConns    int
	idCounter   atomic.Uint64

	// Callbacks
	onConnect    func(*Context)
	onDisconnect func(*Context)
	onReceive    func(*Context, []byte)
}

// NewManager creates a new connection manager
func NewManager(maxConns int) *Manager {
	if maxConns <= 0 {
		maxConns = 10000
	}
	return &Manager{
		maxConns: maxConns,
	}
}

// SetCallbacks sets the callback functions
func (m *Manager) SetCallbacks(onConnect, onDisconnect func(*Context), onReceive func(*Context, []byte)) {
	m.onConnect = onConnect
	m.onDisconnect = onDisconnect
	m.onReceive = onReceive
}

// Add adds a new connection
func (m *Manager) Add(ctx *Context) error {
	if int(m.count.Load()) >= m.maxConns {
		return ErrMaxConnections
	}

	ctx.ID = m.idCounter.Add(1)
	m.connections.Store(ctx.ID, ctx)
	m.count.Add(1)

	if m.onConnect != nil {
		m.onConnect(ctx)
	}

	return nil
}

// Remove removes a connection
func (m *Manager) Remove(ctx *Context) {
	if _, ok := m.connections.LoadAndDelete(ctx.ID); ok {
		m.count.Add(-1)
		if m.onDisconnect != nil {
			m.onDisconnect(ctx)
		}
	}
}

// Get retrieves a connection by ID
func (m *Manager) Get(id uint64) *Context {
	if v, ok := m.connections.Load(id); ok {
		return v.(*Context)
	}
	return nil
}

// Count returns the current connection count
func (m *Manager) Count() int {
	return int(m.count.Load())
}

// Range iterates over all connections
func (m *Manager) Range(fn func(*Context) bool) {
	m.connections.Range(func(key, value interface{}) bool {
		return fn(value.(*Context))
	})
}

// Broadcast sends data to all connections
func (m *Manager) Broadcast(data []byte) {
	m.connections.Range(func(key, value interface{}) bool {
		ctx := value.(*Context)
		if !ctx.IsClosed() {
			_ = ctx.Send(data)
		}
		return true
	})
}

// CloseAll closes all connections
func (m *Manager) CloseAll() {
	m.connections.Range(func(key, value interface{}) bool {
		ctx := value.(*Context)
		_ = ctx.Close()
		return true
	})
}

// OnReceive calls the receive callback
func (m *Manager) OnReceive(ctx *Context, data []byte) {
	if m.onReceive != nil {
		m.onReceive(ctx, data)
	}
}

// UpdateMaxConnections updates the maximum connections limit
func (m *Manager) UpdateMaxConnections(max int) {
	m.maxConns = max
}
