package main

import (
	"apploader/internal/cvm"
	"apploader/internal/secret"
	"log"
	"os"
)

func main() {
	log.SetFlags(log.Lshortfile | log.LstdFlags)
	go secret.StartSecretServer()
	cvm.Start()
	os.Exit(0)
}
