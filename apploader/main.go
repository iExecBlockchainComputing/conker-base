package main

import (
	"apploader/internal/application"
	"apploader/internal/config"
	"log"
	"os"
)

func main() {
	log.SetFlags(log.Lshortfile | log.LstdFlags)
	application.New(config.Load()).Start()
	os.Exit(0)
}
