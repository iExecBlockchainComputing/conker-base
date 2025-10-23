package command

import (
	"bufio"
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
	scanner := bufio.NewScanner(stdout)
	for scanner.Scan() {
		log.Printf("Command output: %s", scanner.Text())
	}
	if err := scanner.Err(); err != nil {
		log.Printf("Error reading command output: %v", err)
	}
	if err = cmd.Wait(); err != nil {
		if ex, ok := err.(*exec.ExitError); ok {
			cmdExitStatus := ex.Sys().(syscall.WaitStatus).ExitStatus()
			log.Printf("Command execution failed with exit status: %d", cmdExitStatus)
		}
		log.Printf("Command execution failed: %v", err)
		return err
	}
	return nil
}
