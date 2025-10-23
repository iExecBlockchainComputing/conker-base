package command

import (
	"fmt"
	"log"
	"os/exec"
	"path"
	"syscall"
)

// RunCommand runs a command
func RunCommand(name string, envs []string, arg ...string) error {
	cmd := exec.Command(name, arg...)

	cmd.Dir = path.Dir(name)

	stdout, err := cmd.StdoutPipe()
	cmd.Stderr = cmd.Stdout
	cmd.Env = envs
	if err != nil {
		return err
	}
	if err = cmd.Start(); err != nil {
		return err
	}
	for {
		tmp := make([]byte, 128)
		_, err := stdout.Read(tmp)
		fmt.Print(string(tmp))
		if err != nil {
			break
		}
	}
	if err = cmd.Wait(); err != nil {
		if ex, ok := err.(*exec.ExitError); ok {
			cmdExitStatus := ex.Sys().(syscall.WaitStatus).ExitStatus()
			log.Println(cmdExitStatus)
		}
		log.Println(err)
		return err
	}
	return nil
}
