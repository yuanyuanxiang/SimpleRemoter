package protocol

import (
	"bytes"
	"encoding/binary"
	"strings"

	"golang.org/x/text/encoding/simplifiedchinese"
	"golang.org/x/text/transform"
)

// gbkToUTF8 converts GBK encoded bytes to UTF-8 string
func gbkToUTF8(data []byte) string {
	// Find the first null byte and truncate there
	if idx := bytes.IndexByte(data, 0); idx >= 0 {
		data = data[:idx]
	}
	if len(data) == 0 {
		return ""
	}

	// Try to decode as GBK
	reader := transform.NewReader(bytes.NewReader(data), simplifiedchinese.GBK.NewDecoder())
	buf := new(bytes.Buffer)
	_, err := buf.ReadFrom(reader)
	if err != nil {
		// If GBK decoding fails, try treating as UTF-8 or ASCII
		return cleanString(string(data))
	}
	return cleanString(buf.String())
}

// cleanString removes non-printable characters except common whitespace
func cleanString(s string) string {
	var result strings.Builder
	for _, r := range s {
		if r >= 32 || r == '\t' || r == '\n' || r == '\r' {
			result.WriteRune(r)
		}
	}
	return strings.TrimSpace(result.String())
}

// Command tokens - matching the C++ definitions
const (
	// Server -> Client commands
	CommandActived    byte = 0   // COMMAND_ACTIVED
	CommandBye        byte = 204 // COMMAND_BYE - disconnect
	CommandHeartbeat  byte = 216 // CMD_HEARTBEAT_ACK

	// Client -> Server tokens
	TokenAuth      byte = 100 // TOKEN_AUTH - authorization required
	TokenHeartbeat byte = 101 // TOKEN_HEARTBEAT
	TokenLogin     byte = 102 // TOKEN_LOGIN - login packet
)

// LOGIN_INFOR structure size and offsets (matching C++ struct with default alignment)
// Note: C++ struct uses default alignment (4-byte for uint32/int)
const (
	LoginInfoSize = 980 // Total size of LOGIN_INFOR struct (with alignment padding)

	// Field offsets (with alignment padding)
	OffsetToken         = 0   // 1 byte (unsigned char)
	OffsetOsVerInfoEx   = 1   // 156 bytes (char[156])
	// 3 bytes padding here to align dwCPUMHz to 4-byte boundary
	OffsetCPUMHz        = 160 // 4 bytes (unsigned int) - aligned to 4
	OffsetModuleVersion = 164 // 24 bytes (char[24])
	OffsetPCName        = 188 // 240 bytes (char[240])
	OffsetMasterID      = 428 // 20 bytes (char[20])
	OffsetWebCamExist   = 448 // 4 bytes (int) - aligned to 4
	OffsetSpeed         = 452 // 4 bytes (unsigned int)
	OffsetStartTime     = 456 // 20 bytes (char[20])
	OffsetReserved      = 476 // 512 bytes (char[512])
)

// LoginInfo represents client login information
type LoginInfo struct {
	Token         byte
	OsVerInfo     string // OS version info
	CPUMHz        uint32
	ModuleVersion string
	PCName        string // Computer name
	MasterID      string
	WebCamExist   bool
	Speed         uint32
	StartTime     string
	Reserved      string // Contains additional info separated by |
}

// ParseLoginInfo parses LOGIN_INFOR from data
func ParseLoginInfo(data []byte) (*LoginInfo, error) {
	if len(data) < 100 { // Minimum size check
		return nil, ErrInvalidData
	}

	info := &LoginInfo{
		Token: data[0],
	}

	// Parse OS version info (offset 1, 156 bytes)
	// The C++ client fills this with a readable string like "Windows 10" via getSystemName()
	if len(data) >= OffsetOsVerInfoEx+156 {
		info.OsVerInfo = parseOsVersionInfo(data[OffsetOsVerInfoEx : OffsetOsVerInfoEx+156])
	}

	// Parse CPU MHz (offset 160, 4 bytes)
	if len(data) >= OffsetCPUMHz+4 {
		info.CPUMHz = binary.LittleEndian.Uint32(data[OffsetCPUMHz:])
	}

	// Parse module version (offset 164, 24 bytes)
	// This contains date string like "Dec 19 2025"
	if len(data) >= OffsetModuleVersion+24 {
		info.ModuleVersion = gbkToUTF8(data[OffsetModuleVersion : OffsetModuleVersion+24])
	}

	// Parse PC name (offset 188, 240 bytes)
	if len(data) >= OffsetPCName+240 {
		info.PCName = gbkToUTF8(data[OffsetPCName : OffsetPCName+240])
	}

	// Parse Master ID (offset 428, 20 bytes)
	if len(data) >= OffsetMasterID+20 {
		info.MasterID = gbkToUTF8(data[OffsetMasterID : OffsetMasterID+20])
	}

	// Parse WebCam exist (offset 448, 4 bytes)
	if len(data) >= OffsetWebCamExist+4 {
		info.WebCamExist = binary.LittleEndian.Uint32(data[OffsetWebCamExist:]) != 0
	}

	// Parse Speed (offset 452, 4 bytes)
	if len(data) >= OffsetSpeed+4 {
		info.Speed = binary.LittleEndian.Uint32(data[OffsetSpeed:])
	}

	// Parse Start time (offset 456, 20 bytes)
	if len(data) >= OffsetStartTime+20 {
		info.StartTime = gbkToUTF8(data[OffsetStartTime : OffsetStartTime+20])
	}

	// Parse Reserved (offset 476, 512 bytes) - contains additional info
	if len(data) >= OffsetReserved+512 {
		info.Reserved = gbkToUTF8(data[OffsetReserved : OffsetReserved+512])
	} else if len(data) > OffsetReserved {
		info.Reserved = gbkToUTF8(data[OffsetReserved:])
	}

	return info, nil
}

// parseOsVersionInfo parses the OS version info field
// The C++ client fills this with a readable string like "Windows 10" via getSystemName()
func parseOsVersionInfo(data []byte) string {
	return gbkToUTF8(data)
}

// ParseReserved parses the reserved field into a slice of strings
func (info *LoginInfo) ParseReserved() []string {
	if info.Reserved == "" {
		return nil
	}
	return strings.Split(info.Reserved, "|")
}

// GetReservedField returns a specific field from reserved data by index
// Fields: ClientType(0), SystemBits(1), CPU(2), Memory(3), FilePath(4),
// Reserved(5), InstallTime(6), InstallInfo(7), ProgramBits(8), ExpiredDate(9),
// ClientLoc(10), ClientPubIP(11), ExeVersion(12), Username(13), IsAdmin(14)
func (info *LoginInfo) GetReservedField(index int) string {
	fields := info.ParseReserved()
	if index >= 0 && index < len(fields) {
		return fields[index]
	}
	return ""
}

// Validation structure for TOKEN_AUTH
type Validation struct {
	From     string // Start date
	To       string // End date
	Admin    string // Admin address
	Port     int    // Admin port
	Checksum string // Reserved field
}

// BuildValidation creates a validation response
func BuildValidation(days float64, admin string, port int) []byte {
	// This would build the validation structure
	// For now, return a simple structure
	data := make([]byte, 160) // Size of Validation struct
	data[0] = TokenAuth

	// Fill in fields...
	// From: 20 bytes
	// To: 20 bytes
	// Admin: 100 bytes
	// Port: 4 bytes
	// Checksum: 16 bytes

	return data
}
