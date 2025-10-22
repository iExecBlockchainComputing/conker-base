package secret

import (
	"net/http"

	"github.com/gin-gonic/gin"
)

// SecretHandler is the handler for the secret service
type SecretHandler struct {
	service SecretService
}

// NewSecretHandler creates a new secret handler
func NewSecretHandler(service SecretService) *SecretHandler {
	return &SecretHandler{service: service}
}

// RegisterHandler registers the secret handler
func (h *SecretHandler) RegisterHandler(router *gin.Engine) {
	router.POST("/secret", h.saveSecret)
}

// SaveSecret saves a secret
func (h *SecretHandler) saveSecret(c *gin.Context) {
	h.service.SaveSecret(c.PostForm("key"), c.PostForm("value"))
	c.JSON(http.StatusOK, gin.H{
		"code":    200,
		"message": "update secret successful",
	})
}
