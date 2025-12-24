package auth

import (
	"bytes"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"os"
	"strings"

	"golang.org/x/text/encoding/simplifiedchinese"
	"golang.org/x/text/transform"
)

// Config holds authentication configuration
type Config struct {
	PwdHash   string // SHA256 hash of the password (64 hex chars)
	SuperPass string // Super admin password for HMAC verification
}

// DefaultConfig returns default auth configuration
func DefaultConfig() *Config {
	return &Config{
		PwdHash:   "", // Must be configured
		SuperPass: "", // Can be set via YAMA_PWD env var
	}
}

// Authenticator handles token authentication
type Authenticator struct {
	config *Config
}

// New creates a new Authenticator
func New(config *Config) *Authenticator {
	return &Authenticator{config: config}
}

// AuthResult contains the result of authentication
type AuthResult struct {
	Valid    bool
	Message  string
	SN       string
	Passcode string
}

// Authenticate validates a TOKEN_AUTH request
// Data format:
//   - offset 0: TOKEN_AUTH command byte
//   - offset 1-19: SN (serial number, 19 bytes)
//   - offset 20-62: Passcode (42 bytes)
//   - offset 62-70: HMAC signature (uint64, 8 bytes) if len > 64
func (a *Authenticator) Authenticate(data []byte) *AuthResult {
	result := &AuthResult{
		Valid:   false,
		Message: "未获授权或消息哈希校验失败",
	}

	// Minimum length check: 1 (token) + 19 (sn) + 1 (at least some passcode)
	if len(data) <= 20 {
		return result
	}

	// Extract SN (bytes 1-19)
	sn := string(data[1:20])
	result.SN = sn

	// Extract passcode (bytes 20-62, or until end if shorter)
	passcodeEnd := 62
	if len(data) < passcodeEnd {
		passcodeEnd = len(data)
	}
	passcode := string(data[20:passcodeEnd])
	result.Passcode = passcode

	// Extract HMAC if present (bytes 62-70)
	var hmacSig uint64
	if len(data) >= 70 {
		hmacSig = binary.LittleEndian.Uint64(data[62:70])
	} else if len(data) > 62 {
		// Partial HMAC data - safely handle incomplete bytes
		hmacBytes := make([]byte, 8)
		copy(hmacBytes, data[62:])
		hmacSig = binary.LittleEndian.Uint64(hmacBytes)
	}

	// Split passcode by '-'
	parts := strings.Split(passcode, "-")
	if len(parts) != 6 && len(parts) != 7 {
		return result
	}

	// Get last 4 parts as subvector
	subvector := parts[len(parts)-4:]

	// Build password string: v[0] + " - " + v[1] + ": " + PwdHash + (optional: ": " + v[2])
	password := parts[0] + " - " + parts[1] + ": " + a.config.PwdHash
	if len(parts) == 7 {
		password += ": " + parts[2]
	}

	// Derive key from password and SN
	finalKey := DeriveKey(password, sn)

	// Get fixed length ID
	hash256 := strings.Join(subvector, "-")
	fixedKey := GetFixedLengthID(finalKey)

	// Debug output (can be removed in production)
	// fmt.Printf("DEBUG: password=%q sn=%q finalKey=%s fixedKey=%s hash256=%s\n", password, sn, finalKey, fixedKey, hash256)

	// Compare
	if hash256 != fixedKey {
		return result
	}

	// Passcode validation successful, now verify HMAC
	superPass := os.Getenv("YAMA_PWD")
	if superPass == "" {
		superPass = a.config.SuperPass
	}

	if superPass != "" && hmacSig != 0 {
		verified := VerifyMessage(superPass, []byte(passcode), hmacSig)
		if verified {
			result.Valid = true
			result.Message = "此程序已获授权，请遵守授权协议，感谢合作"
		}
		// If HMAC verification fails, valid remains false
	} else if hmacSig == 0 {
		// No HMAC provided but passcode is valid - could be older client
		// Keep as invalid for security
	}

	return result
}

// utf8ToGBK converts UTF-8 string to GBK encoded bytes
func utf8ToGBK(s string) []byte {
	reader := transform.NewReader(bytes.NewReader([]byte(s)), simplifiedchinese.GBK.NewEncoder())
	buf := new(bytes.Buffer)
	buf.ReadFrom(reader)
	return buf.Bytes()
}

// BuildResponse builds the 100-byte response for TOKEN_AUTH
func (a *Authenticator) BuildResponse(result *AuthResult) []byte {
	resp := make([]byte, 100)
	if result.Valid {
		resp[0] = 1
	}
	// Message starts at offset 4, convert UTF-8 to GBK for Windows client
	gbkMsg := utf8ToGBK(result.Message)
	copy(resp[4:], gbkMsg)
	return resp
}

// HashSHA256 computes SHA256 hash and returns hex string
func HashSHA256(data string) string {
	h := sha256.New()
	h.Write([]byte(data))
	return hex.EncodeToString(h.Sum(nil))
}

// DeriveKey derives a key from password and hardware ID
// Format: SHA256(password + " + " + hardwareID)
func DeriveKey(password, hardwareID string) string {
	return HashSHA256(password + " + " + hardwareID)
}

// GetFixedLengthID formats a hash into fixed length ID
// Format: xxxx-xxxx-xxxx-xxxx (first 16 chars split by -)
func GetFixedLengthID(hash string) string {
	if len(hash) < 16 {
		return hash
	}
	return hash[0:4] + "-" + hash[4:8] + "-" + hash[8:12] + "-" + hash[12:16]
}

// SignMessage computes HMAC-SHA256 and returns first 8 bytes as uint64
func SignMessage(pwd string, msg []byte) uint64 {
	h := hmac.New(sha256.New, []byte(pwd))
	h.Write(msg)
	hash := h.Sum(nil)
	return binary.LittleEndian.Uint64(hash[:8])
}

// VerifyMessage verifies HMAC signature
func VerifyMessage(pwd string, msg []byte, signature uint64) bool {
	computed := SignMessage(pwd, msg)
	return computed == signature
}

// GenHMAC generates HMAC for password verification
// This matches the C++ genHMAC function
func GenHMAC(pwdHash, superPass string) string {
	key := HashSHA256(superPass)
	list := []string{"g", "h", "o", "s", "t"}
	for _, item := range list {
		key = HashSHA256(key + " - " + item)
	}
	result := HashSHA256(pwdHash + " - " + key)
	if len(result) >= 16 {
		return result[:16]
	}
	return result
}

// HeartbeatAuthResult contains the result of heartbeat authentication
type HeartbeatAuthResult struct {
	Authorized bool
	SN         string
	Passcode   string
	PwdHmac    uint64
}

// AuthenticateHeartbeat validates authorization info from a Heartbeat message
// Data format (after TOKEN_HEARTBEAT byte):
//   - offset 0: Time (8 bytes, uint64)
//   - offset 8: ActiveWnd (512 bytes)
//   - offset 520: Ping (4 bytes, int)
//   - offset 524: HasSoftware (4 bytes, int)
//   - offset 528: SN (20 bytes)
//   - offset 548: Passcode (44 bytes)
//   - offset 592: PwdHmac (8 bytes, uint64)
func (a *Authenticator) AuthenticateHeartbeat(data []byte) *HeartbeatAuthResult {
	result := &HeartbeatAuthResult{
		Authorized: false,
	}

	// Minimum length check: need at least SN + Passcode + PwdHmac
	// Offset 528 + 20 (SN) + 44 (Passcode) + 8 (PwdHmac) = 600 bytes
	if len(data) < 600 {
		return result
	}

	// Extract SN (offset 528, 20 bytes)
	snBytes := data[528:548]
	// Find null terminator
	snEnd := bytes.IndexByte(snBytes, 0)
	if snEnd == -1 {
		snEnd = len(snBytes)
	}
	sn := string(snBytes[:snEnd])
	result.SN = sn

	// Extract Passcode (offset 548, 44 bytes)
	passcodeBytes := data[548:592]
	passcodeEnd := bytes.IndexByte(passcodeBytes, 0)
	if passcodeEnd == -1 {
		passcodeEnd = len(passcodeBytes)
	}
	passcode := string(passcodeBytes[:passcodeEnd])
	result.Passcode = passcode

	// Extract PwdHmac (offset 592, 8 bytes)
	pwdHmac := binary.LittleEndian.Uint64(data[592:600])
	result.PwdHmac = pwdHmac

	// If SN, Passcode, or PwdHmac is empty/zero, not authorized
	if sn == "" || passcode == "" || pwdHmac == 0 {
		return result
	}

	// Split passcode by '-'
	parts := strings.Split(passcode, "-")
	if len(parts) != 6 && len(parts) != 7 {
		return result
	}

	// Get last 4 parts as subvector
	subvector := parts[len(parts)-4:]

	// Build password string: v[0] + " - " + v[1] + ": " + PwdHash + (optional: ": " + v[2])
	password := parts[0] + " - " + parts[1] + ": " + a.config.PwdHash
	if len(parts) == 7 {
		password += ": " + parts[2]
	}

	// Derive key from password and SN
	finalKey := DeriveKey(password, sn)

	// Get fixed length ID
	hash256 := strings.Join(subvector, "-")
	fixedKey := GetFixedLengthID(finalKey)

	// Compare passcode
	if hash256 != fixedKey {
		return result
	}

	// Passcode validation successful, now verify HMAC
	superPass := os.Getenv("YAMA_PWD")
	if superPass == "" {
		superPass = a.config.SuperPass
	}

	if superPass != "" {
		verified := VerifyMessage(superPass, []byte(passcode), pwdHmac)
		if verified {
			result.Authorized = true
		}
	}

	return result
}
