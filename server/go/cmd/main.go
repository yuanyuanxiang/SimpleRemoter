package main

import (
	"flag"
	"fmt"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"

	"github.com/yuanyuanxiang/SimpleRemoter/server/go/auth"
	"github.com/yuanyuanxiang/SimpleRemoter/server/go/connection"
	"github.com/yuanyuanxiang/SimpleRemoter/server/go/logger"
	"github.com/yuanyuanxiang/SimpleRemoter/server/go/protocol"
	"github.com/yuanyuanxiang/SimpleRemoter/server/go/server"
)

// MyHandler implements the server.Handler interface
type MyHandler struct {
	log  *logger.Logger
	auth *auth.Authenticator
	srv  *server.Server
}

// OnConnect is called when a client connects
func (h *MyHandler) OnConnect(ctx *connection.Context) {
	// Only log connection established, detailed info logged on login
}

// OnDisconnect is called when a client disconnects
func (h *MyHandler) OnDisconnect(ctx *connection.Context) {
	info := ctx.GetInfo()
	if info.ClientID != "" {
		h.log.ClientEvent("offline", ctx.ID, ctx.GetPeerIP(),
			"clientID", info.ClientID,
			"computer", info.ComputerName,
		)
	}
}

// OnReceive is called when data is received from a client
func (h *MyHandler) OnReceive(ctx *connection.Context, data []byte) {
	if len(data) == 0 {
		return
	}

	cmd := data[0]
	// Handle commands
	switch cmd {
	case protocol.TokenLogin:
		h.handleLogin(ctx, data)
	case protocol.TokenAuth:
		h.handleAuth(ctx, data)
	case protocol.TokenHeartbeat:
		h.handleHeartbeat(ctx, data)
	default:
		// Other commands are not implemented yet
		h.log.Info("Unhandled command %d from client %d", cmd, ctx.ID)
	}
}

// handleLogin handles client login (TOKEN_LOGIN = 102)
func (h *MyHandler) handleLogin(ctx *connection.Context, data []byte) {
	info, err := protocol.ParseLoginInfo(data)
	if err != nil {
		h.log.Error("Failed to parse login info from client %d: %v", ctx.ID, err)
		return
	}

	// Use MasterID from login request as ClientID for logging
	clientID := info.MasterID
	if clientID == "" {
		clientID = fmt.Sprintf("conn-%d", ctx.ID)
	}

	// Store client info
	reserved := info.ParseReserved()
	clientInfo := connection.ClientInfo{
		ClientID:     clientID,
		ComputerName: info.PCName,
		OS:           info.OsVerInfo,
		Version:      info.ModuleVersion,
		HasCamera:    info.WebCamExist,
		InstallTime:  info.StartTime,
	}

	// Parse additional info from reserved field
	if len(reserved) > 0 {
		clientInfo.ClientType = info.GetReservedField(0)
	}
	if len(reserved) > 2 {
		clientInfo.CPU = info.GetReservedField(2)
	}
	if len(reserved) > 4 {
		clientInfo.FilePath = info.GetReservedField(4)
	}
	if len(reserved) > 11 {
		clientInfo.IP = info.GetReservedField(11) // Public IP
	}

	ctx.SetInfo(clientInfo)
	ctx.IsLoggedIn.Store(true)

	h.log.ClientEvent("online", ctx.ID, ctx.GetPeerIP(),
		"clientID", clientID,
		"computer", info.PCName,
		"os", info.OsVerInfo,
		"version", info.ModuleVersion,
		"path", clientInfo.FilePath,
	)
}

// handleAuth handles authorization request (TOKEN_AUTH = 100)
func (h *MyHandler) handleAuth(ctx *connection.Context, data []byte) {
	result := h.auth.Authenticate(data)
	info := ctx.GetInfo()

	if result.Valid {
		h.log.Info("Auth success: clientID=%s computer=%s ip=%s sn=%s passcode=%s",
			info.ClientID, info.ComputerName, ctx.GetPeerIP(), result.SN, result.Passcode)
	} else {
		h.log.Warn("Auth failed: clientID=%s computer=%s ip=%s sn=%s passcode=%s",
			info.ClientID, info.ComputerName, ctx.GetPeerIP(), result.SN, result.Passcode)
	}

	// Build and send response
	resp := h.auth.BuildResponse(result)
	if err := h.srv.Send(ctx, resp); err != nil {
		h.log.Error("Failed to send auth response to client %d: %v", ctx.ID, err)
	}
}

// handleHeartbeat handles heartbeat from client (TOKEN_HEARTBEAT = 101)
// Heartbeat structure (after command byte):
//   - offset 0: Time (8 bytes, uint64)
//   - offset 8: ActiveWnd (512 bytes)
//   - offset 520: Ping (4 bytes, int)
//   - offset 524: HasSoftware (4 bytes, int)
//   - offset 528: SN (20 bytes)
//   - offset 548: Passcode (44 bytes)
//   - offset 592: PwdHmac (8 bytes, uint64)
//
// HeartbeatACK structure:
//   - offset 0: Time (8 bytes, uint64)
//   - offset 8: Authorized (1 byte, char)
//   - offset 9: Reserved (23 bytes)
func (h *MyHandler) handleHeartbeat(ctx *connection.Context, data []byte) {

	// Parse Time from heartbeat request (offset 1, 8 bytes)
	var hbTime uint64
	if len(data) >= 9 {
		hbTime = uint64(data[1]) | uint64(data[2])<<8 | uint64(data[3])<<16 | uint64(data[4])<<24 |
			uint64(data[5])<<32 | uint64(data[6])<<40 | uint64(data[7])<<48 | uint64(data[8])<<56
	}

	// Authenticate heartbeat if it contains authorization info
	// data[1:] skips the command byte to get the raw Heartbeat structure
	var authorized byte = 0
	if len(data) > 1 {
		authResult := h.auth.AuthenticateHeartbeat(data[1:])
		if authResult.Authorized {
			authorized = 1
			// Log authorization success (only log once per connection to avoid spam)
			if !ctx.IsAuthorized.Load() {
				ctx.IsAuthorized.Store(true)
				info := ctx.GetInfo()
				h.log.Info("Heartbeat auth success: clientID=%s computer=%s ip=%s sn=%s passcode=%s pwdHmac=%d",
					info.ClientID, info.ComputerName, ctx.GetPeerIP(), authResult.SN, authResult.Passcode, authResult.PwdHmac)
			}
		}
	}

	// Build HeartbeatACK response: CMD_HEARTBEAT_ACK(1) + HeartbeatACK(32)
	resp := make([]byte, 33)
	resp[0] = protocol.CommandHeartbeat // CMD_HEARTBEAT_ACK = 216
	// Time at offset 1 (8 bytes, little-endian)
	resp[1] = byte(hbTime)
	resp[2] = byte(hbTime >> 8)
	resp[3] = byte(hbTime >> 16)
	resp[4] = byte(hbTime >> 24)
	resp[5] = byte(hbTime >> 32)
	resp[6] = byte(hbTime >> 40)
	resp[7] = byte(hbTime >> 48)
	resp[8] = byte(hbTime >> 56)
	// Authorized at offset 9 (1 byte)
	resp[9] = authorized
	// Reserved[23] at offset 10 is already zero

	if err := h.srv.Send(ctx, resp); err != nil {
		h.log.Error("Failed to send heartbeat ACK to client %d: %v", ctx.ID, err)
	}
}

// parsePorts parses a semicolon-separated port string and returns port numbers
func parsePorts(portStr string) ([]int, error) {
	var ports []int
	parts := strings.Split(portStr, ";")
	for _, p := range parts {
		p = strings.TrimSpace(p)
		if p == "" {
			continue
		}
		port, err := strconv.Atoi(p)
		if err != nil {
			return nil, fmt.Errorf("invalid port %q: %v", p, err)
		}
		if port < 1 || port > 65535 {
			return nil, fmt.Errorf("port %d out of range (1-65535)", port)
		}
		ports = append(ports, port)
	}
	if len(ports) == 0 {
		return nil, fmt.Errorf("no valid ports specified")
	}
	return ports, nil
}

func main() {
	// Parse command line flags
	portStr := flag.String("port", "6543", "Server listen ports (semicolon-separated, e.g. 6543;6544;6545)")
	flag.StringVar(portStr, "p", "6543", "Server listen ports (shorthand)")
	noConsole := flag.Bool("no-console", false, "Disable console output (for daemon mode)")
	flag.Parse()

	// Parse ports
	ports, err := parsePorts(*portStr)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}

	// Create logger with file output
	logCfg := logger.DefaultConfig()
	logCfg.Level = logger.LevelDebug
	logCfg.Console = !*noConsole
	logCfg.File = "logs/server.log"
	logCfg.MaxSize = 100   // 100 MB
	logCfg.MaxBackups = 10 // keep 10 old files
	logCfg.MaxAge = 30     // 30 days
	logCfg.Compress = true

	log := logger.New(logCfg)

	// Create auth config
	authCfg := auth.DefaultConfig()
	// PwdHash can be set from environment or config file
	authCfg.PwdHash = os.Getenv("YAMA_PWDHASH")
	if authCfg.PwdHash == "" {
		// Default placeholder - should be configured in production
		authCfg.PwdHash = "61f04dd637a74ee34493fc1025de2c131022536da751c29e3ff4e9024d8eec43"
	}
	authCfg.SuperPass = os.Getenv("YAMA_PWD")

	// Create authenticator (shared by all servers)
	authenticator := auth.New(authCfg)

	// Create servers for each port
	var servers []*server.Server
	for _, port := range ports {
		config := server.DefaultConfig()
		config.Port = port
		config.MaxConnections = 9999

		srv := server.New(config)
		srv.SetLogger(log.WithPrefix(fmt.Sprintf("Server:%d", port)))

		// Create handler for this server
		handler := &MyHandler{
			log:  log.WithPrefix(fmt.Sprintf("Handler:%d", port)),
			auth: authenticator,
			srv:  srv,
		}
		srv.SetHandler(handler)

		servers = append(servers, srv)
	}

	// Start all servers
	for _, srv := range servers {
		if err := srv.Start(); err != nil {
			log.Fatal("Failed to start server: %v", err)
		}
	}

	fmt.Printf("Server started on port(s): %v\n", ports)
	fmt.Println("Logs are written to: logs/server.log")
	fmt.Println("Press Ctrl+C to stop...")

	// Wait for interrupt signal
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	<-sigChan

	fmt.Println("\nShutting down...")
	for _, srv := range servers {
		srv.Stop()
	}
	fmt.Println("Server stopped")
}
