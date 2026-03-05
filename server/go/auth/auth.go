package auth

import (
	"bytes"
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"math/big"
	"os"
	"strings"
	"time"

	"golang.org/x/text/encoding/simplifiedchinese"
	"golang.org/x/text/transform"
)

// V2 License Public Key (ECDSA P-256)
// Generated: 2026-03-05 00:48:25
// Same as g_LicensePublicKey in client_verify.h
var V2LicensePublicKey = [64]byte{
	0xa9, 0x5d, 0x1d, 0x44, 0x35, 0x86, 0x85, 0xdd,
	0xbe, 0x27, 0x26, 0x6d, 0xe7, 0x33, 0x27, 0xf8,
	0xbe, 0x2d, 0x87, 0xdd, 0xc1, 0x47, 0x18, 0xbf,
	0xc6, 0x32, 0xfd, 0xce, 0xec, 0x25, 0x1b, 0xf5,
	0x9b, 0x8a, 0x26, 0xa9, 0x85, 0x42, 0x72, 0x9f,
	0x68, 0x79, 0x9b, 0x83, 0x5e, 0x2b, 0xd6, 0x59,
	0x86, 0x64, 0x85, 0xe1, 0xf3, 0xa3, 0x18, 0x95,
	0x5d, 0xd6, 0x3f, 0x2f, 0x55, 0x0b, 0x76, 0xbd,
}

// VerifyV2Signature verifies an ECDSA P-256 signature
// signature: "v2:" prefix followed by base64-encoded 64-byte signature
// sn: serial number
// passcode: license token
// Returns true if signature is valid
func VerifyV2Signature(signature, sn, passcode string) bool {
	// Check "v2:" prefix
	if len(signature) < 4 || signature[:3] != "v2:" {
		return false
	}

	// Decode base64 signature
	sigBase64 := signature[3:]
	sigBytes, err := base64.StdEncoding.DecodeString(sigBase64)
	if err != nil || len(sigBytes) != 64 {
		return false
	}

	// Build payload: "sn|passcode"
	payload := sn + "|" + passcode

	// Compute SHA256 hash
	hash := sha256.Sum256([]byte(payload))

	// Parse public key (X and Y coordinates, 32 bytes each)
	x := new(big.Int).SetBytes(V2LicensePublicKey[:32])
	y := new(big.Int).SetBytes(V2LicensePublicKey[32:])
	pubKey := &ecdsa.PublicKey{
		Curve: elliptic.P256(),
		X:     x,
		Y:     y,
	}

	// Parse signature (R and S, 32 bytes each)
	r := new(big.Int).SetBytes(sigBytes[:32])
	s := new(big.Int).SetBytes(sigBytes[32:])

	// Verify signature
	return ecdsa.Verify(pubKey, hash[:], r, s)
}

// IsV2Signature checks if the signature has "v2:" prefix
func IsV2Signature(sig string) bool {
	return len(sig) >= 3 && sig[:3] == "v2:"
}

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
	IsV2     bool  // true if V2 verification was used
}

// Authenticate validates a TOKEN_AUTH request
// Data format (V1):
//   - offset 0: TOKEN_AUTH command byte
//   - offset 1-19: SN (serial number, 19 bytes)
//   - offset 20-61: Passcode (42 bytes)
//   - offset 62-69: HMAC signature (uint64, 8 bytes)
//
// Data format (V2):
//   - offset 0: TOKEN_AUTH command byte
//   - offset 1-19: SN (serial number, 19 bytes)
//   - offset 20-61: Passcode (42 bytes)
//   - offset 62-69: HMAC = 0 (8 bytes, indicates V2)
//   - offset 70-79: Version (10 bytes)
//   - offset 80+: V2 signature ("v2:BASE64...", null-terminated)
func (a *Authenticator) Authenticate(data []byte) *AuthResult {
	result := &AuthResult{
		Valid:   false,
		Message: "未获授权或消息哈希校验失败",
	}

	// Minimum length check: 1 (token) + 19 (sn) + 1 (at least some passcode)
	if len(data) <= 20 {
		return result
	}

	// Extract SN (bytes 1-19), trim null bytes
	snBytes := data[1:20]
	snEnd := bytes.IndexByte(snBytes, 0)
	if snEnd == -1 {
		snEnd = len(snBytes)
	}
	sn := string(snBytes[:snEnd])
	result.SN = sn

	// Extract passcode (bytes 20-62, or until end if shorter), trim null bytes
	passcodeEnd := 62
	if len(data) < passcodeEnd {
		passcodeEnd = len(data)
	}
	passcodeBytes := data[20:passcodeEnd]
	pcEnd := bytes.IndexByte(passcodeBytes, 0)
	if pcEnd == -1 {
		pcEnd = len(passcodeBytes)
	}
	passcode := string(passcodeBytes[:pcEnd])
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

	// Extract V2 signature if HMAC is 0 and data is long enough
	// V2 signature starts at offset 80 (after 10-byte version field)
	var hmacV2 string
	if hmacSig == 0 && len(data) > 80 {
		v2Bytes := data[80:]
		v2End := bytes.IndexByte(v2Bytes, 0)
		if v2End == -1 {
			v2End = len(v2Bytes)
		}
		hmacV2 = string(v2Bytes[:v2End])
	}

	// Check for V2 verification first
	if IsV2Signature(hmacV2) {
		result.IsV2 = true
		// V2 verification using ECDSA
		if VerifyV2Signature(hmacV2, sn, passcode) {
			// Signature verified, now check date range
			parts := strings.Split(passcode, "-")
			if len(parts) >= 2 {
				if !isWithinDateRange(parts[0], parts[1]) {
					result.Message = "授权已过期或尚未生效"
					return result
				}
				result.Valid = true
				result.Message = "此程序已获授权，请遵守授权协议，感谢合作"
			}
		}
		return result
	}

	// V1 verification: validate passcode structure first
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
			// HMAC verified, now check date range
			// passcode format: YYYYMMDD-YYYYMMDD-xxx (first two parts are start and end dates)
			if !isWithinDateRange(parts[0], parts[1]) {
				result.Message = "授权已过期或尚未生效"
				return result
			}
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

// isWithinDateRange checks if current date is within the specified date range
// startDate and endDate are in YYYYMMDD format (e.g., "20251231")
func isWithinDateRange(startDate, endDate string) bool {
	const dateFormat = "20060102" // Go reference time format for YYYYMMDD

	start, err := time.Parse(dateFormat, startDate)
	if err != nil {
		return false
	}

	end, err := time.Parse(dateFormat, endDate)
	if err != nil {
		return false
	}

	// Set end date to end of day (23:59:59)
	end = end.Add(24*time.Hour - time.Second)

	now := time.Now()
	return !now.Before(start) && !now.After(end)
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
	PwdHmac    uint64  // V1 HMAC
	PwdHmacV2  string  // V2 signature (if used)
	IsV2       bool    // true if V2 verification was used
}

// AuthenticateHeartbeat validates authorization info from a Heartbeat message
// Data format (after TOKEN_HEARTBEAT byte):
//   - offset 0: Time (8 bytes, uint64)
//   - offset 8: ActiveWnd (512 bytes)
//   - offset 520: Ping (4 bytes, int)
//   - offset 524: HasSoftware (4 bytes, int)
//   - offset 528: SN (20 bytes)
//   - offset 548: Passcode (44 bytes)
//   - offset 592: PwdHmac (8 bytes, uint64) - V1 HMAC, if 0 check PwdHmacV2
//   - offset 600: PwdHmacV2 (96 bytes) - V2 signature "v2:BASE64..."
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

	// If SN or Passcode is empty, not authorized
	if sn == "" || passcode == "" {
		return result
	}

	// Extract PwdHmacV2 if PwdHmac is 0 and data is long enough
	// offset 600: PwdHmacV2 (96 bytes)
	var hmacV2 string
	if pwdHmac == 0 && len(data) >= 696 {
		v2Bytes := data[600:696]
		v2End := bytes.IndexByte(v2Bytes, 0)
		if v2End == -1 {
			v2End = len(v2Bytes)
		}
		hmacV2 = string(v2Bytes[:v2End])
	}

	// Check for V2 verification first
	if IsV2Signature(hmacV2) {
		result.PwdHmacV2 = hmacV2
		result.IsV2 = true
		// V2 verification using ECDSA
		if VerifyV2Signature(hmacV2, sn, passcode) {
			// Signature verified, now check date range
			parts := strings.Split(passcode, "-")
			if len(parts) >= 2 && isWithinDateRange(parts[0], parts[1]) {
				result.Authorized = true
			}
		}
		return result
	}

	// V1 verification: PwdHmac must be non-zero
	if pwdHmac == 0 {
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
			// HMAC verified, now check date range
			// passcode format: YYYYMMDD-YYYYMMDD-xxx (first two parts are start and end dates)
			if isWithinDateRange(parts[0], parts[1]) {
				result.Authorized = true
			}
		}
	}

	return result
}
