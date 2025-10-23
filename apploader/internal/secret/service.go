package secret

import "sync"

// SecretService is the interface for the secret service
type SecretService interface {
	SaveSecret(key string, value string)
	GetAllSecrets() map[string]string
}

// secretService is the implementation of the SecretService interface
type secretService struct {
	secrets      map[string]string
	secretsMutex sync.RWMutex
}

// NewSecretService creates a new secret service
func NewSecretService() SecretService {
	return &secretService{
		secrets: make(map[string]string),
	}
}

// SaveSecret saves a secret
func (s *secretService) SaveSecret(key string, value string) {
	s.secretsMutex.Lock()
	defer s.secretsMutex.Unlock()
	s.secrets[key] = value
}

// GetAllSecrets gets all secrets
func (s *secretService) GetAllSecrets() map[string]string {
	s.secretsMutex.RLock()
	defer s.secretsMutex.RUnlock()
	return s.secrets
}
