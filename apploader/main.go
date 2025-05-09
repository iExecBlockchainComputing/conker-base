package main

import (
	csvapp "apploader/csv-app"
	"apploader/secret_server"
	"log"
	"os"
)

func main() {
	log.SetFlags(log.Lshortfile | log.LstdFlags)
	go secret_server.StartSecretServer()
	csvapp.Start()
	os.Exit(0)
}
