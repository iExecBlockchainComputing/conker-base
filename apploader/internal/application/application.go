package application

import (
	"apploader/internal/config"
	"apploader/internal/cvm"
	"apploader/internal/secret"
	"context"
	"net"
	"os"
	"os/signal"
	"syscall"
	"time"

	"log"
	"net/http"

	"github.com/gin-gonic/gin"
)

// Application orchestrates CVM boot sequence, secrets, and HTTP server.
type Application struct {
	config         *config.Config
	cvmBootManager cvm.CvmBootManager
	secretService  secret.SecretService
	server         *http.Server
}

// New creates a new application
func New(cfg *config.Config) *Application {
	secretService := secret.NewSecretService()
	cvmBootManager, err := cvm.NewCvmBootManager(&cfg.Cvm, secretService)
	if err != nil {
		log.Fatalf("Failed to create CVM boot manager: %v", err)
	}
	return &Application{
		config:         cfg,
		secretService:  secretService,
		cvmBootManager: cvmBootManager,
	}
}

// Start starts the application
func (app *Application) Start() {
	// Channel to signal when API is ready
	apiReady := make(chan struct{})
	go app.startHTTP(apiReady)
	<-apiReady
	log.Printf("API started successfully")

	// Channel to signal when CVM boot sequence is done
	cvmDone := make(chan bool, 1)

	// Start CVM boot sequence in background
	go func() {
		log.Printf("Starting CVM boot sequence")
		app.cvmBootManager.Start()
		log.Printf("CVM boot sequence completed successfully")
		cvmDone <- true
	}()

	// Setup graceful shutdown
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)

	select {
	case <-cvmDone:
		log.Printf("CVM boot sequence completed. Application will exit.")
	case <-quit:
		log.Printf("Shutting down application...")
	}

	// Graceful shutdown of HTTP server
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	if err := app.server.Shutdown(ctx); err != nil {
		log.Printf("Server forced to shutdown: %v", err)
	}
}

// StartHTTP starts the HTTP server
func (app *Application) startHTTP(ready chan<- struct{}) {
	log.Printf("Starting HTTP server on port %s", app.config.Server.Port)

	router := gin.Default()

	router.GET("/health", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{"status": "ok"})
	})
	secret.NewSecretHandler(app.secretService).RegisterHandler(router)

	app.server = &http.Server{
		Addr:         app.config.Server.Port,
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 10 * time.Second,
		IdleTimeout:  10 * time.Second,

		Handler: router,
	}
	listener, err := net.Listen("tcp", app.config.Server.Port)
	if err != nil {
		log.Fatalf("Failed to start server: %v", err)
	}
	close(ready)

	app.server.Serve(listener)
}
