package doc2pdf

import (
	"bytes"
	"fmt"
	"os/exec"
	"runtime"
	"sync"
)

var mutex sync.Mutex

// libreoffice  ubuntu: apt-get install libreoffice
// /usr/share/fonts
//
//	ConvertToPDF
//	@Description:pdf
//	@param filePath
//	@param outPathPD
//	@return string
func ConvertToPDF(filePath string, outPath string) error {
	//libreoffice
	mutex.Lock()
	defer mutex.Unlock()
	// 1
	commandName := ""
	var params []string
	if runtime.GOOS == "windows" {
		commandName = "cmd"
		params = []string{"/c", "soffice", "--headless", "--invisible", "--convert-to", "pdf", filePath, "--outdir", outPath}
	} else if runtime.GOOS == "linux" {
		commandName = "libreoffice"
		params = []string{"--invisible", "--headless", "--convert-to", "pdf", filePath, "--outdir", outPath}
	}
	//
	if mesg, err := interactiveToexec(commandName, params); err != nil {
		return fmt.Errorf("conver failed, stderr:%s ;error: %s", mesg, err)
	} else {
		return nil
	}
}

func ConvertToImage(filePath string, outPath string) error {
	//libreoffice
	mutex.Lock()
	defer mutex.Unlock()
	// 1
	commandName := ""
	var params []string
	if runtime.GOOS == "windows" {
		commandName = "cmd"
		params = []string{"/c", "soffice", "--headless", "--invisible", "--convert-to", "jpg", filePath, "--outdir", outPath}
	} else if runtime.GOOS == "linux" {
		commandName = "libreoffice"
		params = []string{"--invisible", "--headless", "--convert-to", "jpg", filePath, "--outdir", outPath}
	}
	//
	if mesg, err := interactiveToexec(commandName, params); err != nil {
		return fmt.Errorf("conver failed, stderr:%s ;error: %s", mesg, err)
	} else {
		return nil
	}
}

// interactiveToexec
// @Description:
// @param commandName
// @param params
// @return string
// @return bool
func interactiveToexec(commandName string, params []string) (string, error) {
	cmd := exec.Command(commandName, params...)
	buf, err := cmd.Output()
	w := bytes.NewBuffer(nil)
	cmd.Stderr = w
	if err != nil {
		return string(buf), err
	} else {
		return "", nil
	}
}
