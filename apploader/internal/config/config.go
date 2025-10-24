package config

import "os"

// Config holds application configuration
type Config struct {
	Server ServerConfig
	Cvm    CvmConfig
}

// ServerConfig holds server configuration
type ServerConfig struct {
	Port string
}

// CvmConfig holds cvm configuration
type CvmConfig struct {
	ConfigPath             string
	SupervisorPath         string
	SupervisorTemplatePath string
}

// Load loads configuration from environment variables
func Load() *Config {
	return &Config{
		Server: ServerConfig{
			Port: getEnv("PORT", ":9090"),
		},
		Cvm: CvmConfig{
			ConfigPath:             getEnv("CVM_CONFIG_PATH", "/workplace/apploader/conf/app.yml"),
			SupervisorPath:         getEnv("SUPERVISOR_PATH", "/workplace/supervisord/apploader"),
			SupervisorTemplatePath: getEnv("SUPERVISOR_TEMPLATE_PATH", "conf/supervisord.ini.template"),
		},
	}
}

// getEnv gets an environment variable with a fallback default value
func getEnv(key, defaultValue string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return defaultValue
}
