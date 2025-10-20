package main

import (
	"apploader/internal/config"
	"apploader/internal/cvm"
	"apploader/internal/secret"
	"log"
	"os"
	"time"
)

func main() {
	log.SetFlags(log.Lshortfile | log.LstdFlags)

	cfg := config.Load()
	cvm, err := cvm.NewCvmService(&cfg.Cvm)
	if err != nil {
		log.Fatalf("failed to create cvm service: %v", err)
	}
	go secret.StartSecretServer()
	time.Sleep(2 * time.Second) // wait for secret server to start
	cvm.Start()
	os.Exit(0)
}
