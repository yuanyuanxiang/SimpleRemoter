package logger

import (
	"io"
	"os"
	"path/filepath"
	"time"

	"github.com/rs/zerolog"
	"gopkg.in/natefinch/lumberjack.v2"
)

// Level represents log level
type Level = zerolog.Level

const (
	LevelDebug = zerolog.DebugLevel
	LevelInfo  = zerolog.InfoLevel
	LevelWarn  = zerolog.WarnLevel
	LevelError = zerolog.ErrorLevel
	LevelFatal = zerolog.FatalLevel
)

// Logger wraps zerolog.Logger
type Logger struct {
	zl zerolog.Logger
}

// Config holds logger configuration
type Config struct {
	// Level is the minimum log level
	Level Level
	// Console enables console output
	Console bool
	// File is the log file path (empty to disable file logging)
	File string
	// MaxSize is the max size in MB before rotation
	MaxSize int
	// MaxBackups is the max number of old log files to keep
	MaxBackups int
	// MaxAge is the max days to keep old log files
	MaxAge int
	// Compress enables gzip compression for rotated files
	Compress bool
}

// DefaultConfig returns default configuration
func DefaultConfig() Config {
	return Config{
		Level:      LevelInfo,
		Console:    true,
		File:       "",
		MaxSize:    100,
		MaxBackups: 3,
		MaxAge:     30,
		Compress:   true,
	}
}

// New creates a new logger with config
func New(cfg Config) *Logger {
	var writers []io.Writer

	// Console output with color
	if cfg.Console {
		consoleWriter := zerolog.ConsoleWriter{
			Out:        os.Stdout,
			TimeFormat: "2006-01-02 15:04:05",
		}
		writers = append(writers, consoleWriter)
	}

	// File output with rotation
	if cfg.File != "" {
		// Ensure directory exists
		dir := filepath.Dir(cfg.File)
		if dir != "" && dir != "." {
			_ = os.MkdirAll(dir, 0755)
		}

		fileWriter := &lumberjack.Logger{
			Filename:   cfg.File,
			MaxSize:    cfg.MaxSize,
			MaxBackups: cfg.MaxBackups,
			MaxAge:     cfg.MaxAge,
			Compress:   cfg.Compress,
			LocalTime:  true,
		}
		writers = append(writers, fileWriter)
	}

	// Combine writers
	var writer io.Writer
	if len(writers) == 0 {
		writer = os.Stdout
	} else if len(writers) == 1 {
		writer = writers[0]
	} else {
		writer = zerolog.MultiLevelWriter(writers...)
	}

	// Create logger
	zl := zerolog.New(writer).
		Level(cfg.Level).
		With().
		Timestamp().
		Logger()

	return &Logger{zl: zl}
}

// WithPrefix returns a new logger with a prefix field
func (l *Logger) WithPrefix(prefix string) *Logger {
	return &Logger{
		zl: l.zl.With().Str("module", prefix).Logger(),
	}
}

// Debug logs a debug message
func (l *Logger) Debug(format string, args ...interface{}) {
	l.zl.Debug().Msgf(format, args...)
}

// Info logs an info message
func (l *Logger) Info(format string, args ...interface{}) {
	l.zl.Info().Msgf(format, args...)
}

// Warn logs a warning message
func (l *Logger) Warn(format string, args ...interface{}) {
	l.zl.Warn().Msgf(format, args...)
}

// Error logs an error message
func (l *Logger) Error(format string, args ...interface{}) {
	l.zl.Error().Msgf(format, args...)
}

// Fatal logs a fatal message and exits
func (l *Logger) Fatal(format string, args ...interface{}) {
	l.zl.Fatal().Msgf(format, args...)
}

// SetLevel sets the log level
func (l *Logger) SetLevel(level Level) {
	l.zl = l.zl.Level(level)
}

// GetLevel returns the current log level
func (l *Logger) GetLevel() Level {
	return l.zl.GetLevel()
}

// ClientEvent logs client online/offline events
func (l *Logger) ClientEvent(event string, clientID uint64, ip string, extra ...string) {
	e := l.zl.Info().
		Str("event", event).
		Uint64("client_id", clientID).
		Str("ip", ip).
		Time("time", time.Now())

	if len(extra) >= 2 {
		for i := 0; i+1 < len(extra); i += 2 {
			e = e.Str(extra[i], extra[i+1])
		}
	}

	e.Msg("")
}

// default global logger
var defaultLogger = New(DefaultConfig())

// SetDefault sets the default global logger
func SetDefault(l *Logger) {
	defaultLogger = l
}

// Default returns the default global logger
func Default() *Logger {
	return defaultLogger
}

// Package-level convenience functions

func Debug(format string, args ...interface{}) {
	defaultLogger.Debug(format, args...)
}

func Info(format string, args ...interface{}) {
	defaultLogger.Info(format, args...)
}

func Warn(format string, args ...interface{}) {
	defaultLogger.Warn(format, args...)
}

func Error(format string, args ...interface{}) {
	defaultLogger.Error(format, args...)
}

func Fatal(format string, args ...interface{}) {
	defaultLogger.Fatal(format, args...)
}
