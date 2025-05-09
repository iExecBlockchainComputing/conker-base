package main

import (
	"attest-helper/routers"
	"fmt"
	"os"
	"path"
	"runtime"

	"github.com/sirupsen/logrus"
)

func main() {
	logrus.SetLevel(logrus.TraceLevel)
	logrus.SetFormatter(&logrus.TextFormatter{
		FullTimestamp: true,
		ForceColors:   true,
		CallerPrettyfier: func(f *runtime.Frame) (string, string) {
			filename := path.Base(f.File)
			return "", filename + ":" + fmt.Sprintf("%d", f.Line)
		},
	})
	logrus.SetReportCaller(true)

	port := os.Getenv("port")
	if port == "" {
		port = "8080"
	}
	logrus.Info("attest-helper is running on port: ", port)
	err := routers.InitRouters().Run(fmt.Sprintf(":%s", port))
	if err != nil {
		panic("start server failed, error: " + err.Error())
	}
}
