package protocol

// Header encoding/decoding functions
// Ported from common/header.h and common/encfuncs.h

const (
	MsgHeader   = "HELL"
	FlagCompLen = 4
	FlagLength  = 8
	HdrLength   = FlagLength + 8 // FLAG_LENGTH + 2 * sizeof(uint32)
	MinComLen   = 12
)

// HeaderEncType represents the encryption method used for header
type HeaderEncType int

const (
	HeaderEncUnknown HeaderEncType = -1
	HeaderEncNone    HeaderEncType = 0
	HeaderEncV0      HeaderEncType = 1
	HeaderEncV1      HeaderEncType = 2
	HeaderEncV2      HeaderEncType = 3
	HeaderEncV3      HeaderEncType = 4
	HeaderEncV4      HeaderEncType = 5
	HeaderEncV5      HeaderEncType = 6
	HeaderEncV6      HeaderEncType = 7
)

// DecryptFunc is the function signature for header decryption
type DecryptFunc func(data []byte, key byte)

// defaultDecrypt does nothing (no encryption)
func defaultDecrypt(data []byte, key byte) {
	// No-op
}

// decrypt is the default encryption method (V0)
func decrypt(data []byte, key byte) {
	if key == 0 {
		return
	}
	for i := 0; i < len(data); i++ {
		k := key ^ byte(i*31)
		value := int(data[i])
		switch i % 4 {
		case 0:
			value -= int(k)
		case 1:
			value = value ^ int(k)
		case 2:
			value += int(k)
		case 3:
			value = ^value ^ int(k)
		}
		data[i] = byte(value & 0xFF)
	}
}

// decryptV1 - alternating add/subtract
func decryptV1(data []byte, key byte) {
	for i := 0; i < len(data); i++ {
		if i%2 == 0 {
			data[i] = data[i] - key
		} else {
			data[i] = data[i] + key
		}
	}
}

// decryptV2 - XOR with rotation
func decryptV2(data []byte, key byte) {
	for i := 0; i < len(data); i++ {
		if i%2 == 0 {
			data[i] = (data[i] >> 1) | (data[i] << 7) // rotate right
		} else {
			data[i] = (data[i] << 1) | (data[i] >> 7) // rotate left
		}
		data[i] ^= key
	}
}

// decryptV3 - dynamic key with position
func decryptV3(data []byte, key byte) {
	for i := 0; i < len(data); i++ {
		dynamicKey := key + byte(i%8)
		switch i % 3 {
		case 0:
			data[i] = (data[i] ^ dynamicKey) - dynamicKey
		case 1:
			data[i] = (data[i] + dynamicKey) ^ dynamicKey
		case 2:
			data[i] = ^data[i] - dynamicKey
		}
	}
}

// decryptV4 - pseudo-random XOR (symmetric)
func decryptV4(data []byte, key byte) {
	rand := uint16(key)
	for i := 0; i < len(data); i++ {
		rand = (rand*13 + 17) % 256
		data[i] ^= byte(rand)
	}
}

// decryptV5 - dynamic key with bit shift
func decryptV5(data []byte, key byte) {
	for i := 0; i < len(data); i++ {
		dynamicKey := (key + byte(i)) ^ 0x55
		data[i] = ((data[i] - byte(i%7)) ^ (dynamicKey << 3)) - dynamicKey
	}
}

// decryptV6 - pseudo-random with position (symmetric)
func decryptV6(data []byte, key byte) {
	rand := uint16(key)
	for i := 0; i < len(data); i++ {
		rand = (rand*31 + 17) % 256
		data[i] ^= byte(rand) + byte(i)
	}
}

// All decrypt methods
var decryptMethods = []DecryptFunc{
	defaultDecrypt,
	decrypt,
	decryptV1,
	decryptV2,
	decryptV3,
	decryptV4,
	decryptV5,
	decryptV6,
}

// compare decrypts and compares the flag with magic
func compare(flag []byte, magic string, length int, dec DecryptFunc, key byte) bool {
	if len(flag) < MinComLen {
		return false
	}

	buf := make([]byte, MinComLen)
	copy(buf, flag[:MinComLen])
	dec(buf[:length], key)

	return string(buf[:length]) == magic
}

// CheckHead tries all decryption methods to identify the protocol
func CheckHead(flag []byte) (flagType FlagType, encType HeaderEncType, decrypted []byte) {
	if len(flag) < MinComLen {
		return FlagUnknown, HeaderEncUnknown, nil
	}

	for i, method := range decryptMethods {
		buf := make([]byte, MinComLen)
		copy(buf, flag[:MinComLen])

		ft := checkHeadWithMethod(buf, method)
		if ft != FlagUnknown {
			return ft, HeaderEncType(i), buf
		}
	}

	return FlagUnknown, HeaderEncUnknown, nil
}

// checkHeadWithMethod checks the flag with a specific decrypt method
func checkHeadWithMethod(flag []byte, dec DecryptFunc) FlagType {
	// Try HELL (FLAG_HELL)
	if len(flag) >= FlagLength {
		buf := make([]byte, MinComLen)
		copy(buf, flag)
		key := buf[6]
		dec(buf[:FlagCompLen], key)
		if string(buf[:4]) == MsgHeader {
			copy(flag, buf)
			return FlagHell
		}
	}

	// Try Shine (FLAG_SHINE)
	if len(flag) >= 5 {
		buf := make([]byte, MinComLen)
		copy(buf, flag)
		dec(buf[:5], 0)
		if string(buf[:5]) == "Shine" {
			copy(flag, buf)
			return FlagShine
		}
	}

	// Try <<FUCK>> (FLAG_FUCK)
	if len(flag) >= 10 {
		buf := make([]byte, MinComLen)
		copy(buf, flag)
		key := buf[9]
		dec(buf[:8], key)
		if string(buf[:8]) == "<<FUCK>>" {
			copy(flag, buf)
			return FlagFuck
		}
	}

	// Try Hello? (FLAG_HELLO)
	if len(flag) >= 7 {
		buf := make([]byte, MinComLen)
		copy(buf, flag)
		key := buf[6]
		dec(buf[:6], key)
		if string(buf[:6]) == "Hello?" {
			copy(flag, buf)
			return FlagHello
		}
	}

	return FlagUnknown
}

// FlagType represents the protocol type
type FlagType int

const (
	FlagWinOS   FlagType = -1
	FlagUnknown FlagType = 0
	FlagShine   FlagType = 1
	FlagFuck    FlagType = 2
	FlagHello   FlagType = 3
	FlagHell    FlagType = 4
)

func (f FlagType) String() string {
	switch f {
	case FlagWinOS:
		return "WinOS"
	case FlagShine:
		return "Shine"
	case FlagFuck:
		return "Fuck"
	case FlagHello:
		return "Hello"
	case FlagHell:
		return "Hell"
	default:
		return "Unknown"
	}
}

// GetFlagLength returns the flag length for a given flag type
func GetFlagLength(ft FlagType) int {
	switch ft {
	case FlagShine:
		return 5
	case FlagFuck:
		return 11 // 8 + 3
	case FlagHello:
		return 8
	case FlagHell:
		return FlagLength
	default:
		return 0
	}
}

// GetHeaderLength returns the full header length for a given flag type
func GetHeaderLength(ft FlagType) int {
	return GetFlagLength(ft) + 8
}
