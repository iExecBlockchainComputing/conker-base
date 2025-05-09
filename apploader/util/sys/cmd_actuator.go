package sys

import (
	"bufio"
	"bytes"
	"io"
	"os"
	"os/exec"
	"syscall"
)

func ActuatorCmdWithResult(name string, arg []string) (stdout string, stderr string, err error) {
	cmd := exec.Command(name, arg...)
	stdoutBuf := bytes.Buffer{}
	stderrBuf := bytes.Buffer{}
	cmd.Stdout = &stdoutBuf //
	cmd.Stderr = &stderrBuf //
	err = cmd.Run()
	if err != nil {
		return "", "", err
	}

	return string(stdoutBuf.Bytes()), string(stderrBuf.Bytes()), nil
}

func ActuatorCmdWithErrorCode(name string, arg ...string) (code int) {
	command := exec.Command(name, arg...)
	outinfo := bytes.Buffer{}
	command.Stdout = &outinfo
	var cmdExitStatus int
	err := command.Run()
	if err != nil {
		if ex, ok := err.(*exec.ExitError); ok {
			cmdExitStatus = ex.Sys().(syscall.WaitStatus).ExitStatus()
		}
	}
	return cmdExitStatus
}

func RunCmdWithLog(logPath string, name string, arg ...string) error {
	cmd := exec.Command(name, arg...)

	stdout, _ := cmd.StdoutPipe()
	stderr, _ := cmd.StderrPipe()
	multiReader := io.MultiReader(stdout, stderr)
	readerstd := bufio.NewReader(multiReader)

	//
	if err := cmd.Start(); err != nil {
		return err
	}

	for {
		stdout, err := os.OpenFile(logPath, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0600)
		if err != nil {
			return err
		}
		lineStd, err2 := readerstd.ReadString('\n')
		if err2 != nil || io.EOF == err2 {
			break
		}
		stdout.WriteString(lineStd)
		stdout.Close()
	}
	err := cmd.Wait()
	if err != nil {
		return err
	}
	return nil
}
