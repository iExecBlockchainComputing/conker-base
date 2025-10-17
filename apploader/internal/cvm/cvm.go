package cvm

/*
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
*/
import "C"
import (
	"apploader/internal/secret"
	"apploader/pkg/conversion"
	"apploader/pkg/file"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path"
	"strings"
	"syscall"
	"text/template"
	"time"
	"unsafe"

	"gopkg.in/yaml.v3"
)

const (
	JOB       = "job"
	SERVER    = "server"
	DockerApp = "dockerApp"
	APP       = "app"
)

const (
	SUPERVISOR_PATH = "/workplace/supervisord/apploader"
)

func DoJob(ca *TaskInfo) error {
	if ca.Type != JOB {
		return fmt.Errorf("this task is not a job")
	}

	envs := make([]string, 0)
	for k, v := range secret.Secret {
		envs = append(envs, fmt.Sprintf("%s=%s", k, v))
	}

	if ca.Env != nil {
		userEnv, ok := ca.Env.(map[string]interface{})
		if !ok {
			return fmt.Errorf("user env format error")
		}

		for k, v := range userEnv {
			envs = append(envs, fmt.Sprintf("%s=%s", k, v))
		}
	}

	log.Printf("entrypoint is %s", ca.Entrypoint)
	return RunCommand(ca.Entrypoint, envs, ca.Args...)

}

func RunCommand(name string, envs []string, arg ...string) error {
	cmd := exec.Command(name, arg...)

	cmd.Dir = path.Dir(name)
	//
	stdout, err := cmd.StdoutPipe()
	cmd.Stderr = cmd.Stdout
	cmd.Env = envs
	if err != nil {
		return err
	}
	if err = cmd.Start(); err != nil {
		return err
	}
	//
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

func Execv(main string, args ...string) {
	workdir := path.Dir(main)
	if err := os.Chdir(workdir); err != nil {
		log.Fatalf("change to work dir failed, error: %s\n", err.Error())
	}

	sizeOfArgs_C := len(args) + 2 // +2 for [0]=main [1:end]=args [end]=nil
	Args_C := make([]*C.char, sizeOfArgs_C)
	Args_C[0] = C.CString(main)
	defer C.free(unsafe.Pointer(Args_C[0]))
	for i, arg := range args {
		Args_C[i+1] = C.CString(arg)
		defer C.free(unsafe.Pointer(Args_C[i+1]))
	}
	Args_C[sizeOfArgs_C-1] = nil
	re := C.execv(C.CString(main), (**C.char)(unsafe.Pointer(&Args_C[0])))
	if re != 0 {
		log.Fatalf("execv %s failed, code is %d\n", main, re)
	}
	return
}

func ExecvDockerApp(ca *TaskInfo) {
	if ca.Type != DockerApp {
		log.Fatalf("task is not a docker app")
	}

	for k, v := range secret.Secret {
		err := os.Setenv(k, v)
		if err != nil {
			log.Fatalf("set secret env failed, error: %s\n", err.Error())
		}
	}

	if ca.Env != nil {
		userEnv, ok := ca.Env.(map[string]interface{})
		if !ok {
			log.Fatalf("user env format error")
		}

		for k, v := range userEnv {
			err := os.Setenv(k, v.(string))
			if err != nil {
				log.Fatalf("set app env failed, error: %s\n", err.Error())
			}
		}
	}

	Execv(ca.Entrypoint, ca.Args...)
}

func CreateSevers(ca *TaskInfo) error {
	if ca.Type != SERVER {
		return fmt.Errorf("task is not a server")
	}

	envs := make([]string, 0)

	// fixme: envs in supervisord will write to file of supervisord.ini, it will expose the secret envs, so deleted
	//for k, v := range secret_server.Secret {
	//	envs = append(envs, fmt.Sprintf("%s=%s", k, v))
	//}

	if ca.Env != nil {
		userEnv, ok := ca.Env.(map[string]interface{})
		if !ok {
			return fmt.Errorf("user env format error")
		}
		for k, v := range userEnv {
			value := conversion.Interface2String(v)
			envs = append(envs, fmt.Sprintf("%s=%s", k, value))
		}
	}

	sConf := new(SupervisorConf)
	sConf.Name = ca.Name
	sConf.Command = ca.Entrypoint + " " + strings.Join(ca.Args, " ")
	sConf.Workplace = path.Dir(ca.Entrypoint)
	sConf.Environment = strings.Join(envs, ",")
	sConf.Priority = ca.Priority

	tmpl, err := template.ParseFiles("conf/supervisord.ini.template")
	if err != nil {
		return fmt.Errorf("parse supervisord template file failed, error: %s\n", err.Error())
	}

	supervisordINIPath := path.Join(SUPERVISOR_PATH, fmt.Sprintf("%s.ini", ca.Name))
	if file.IsFile(supervisordINIPath) {
		os.RemoveAll(supervisordINIPath)
	}

	f, err := os.Create(supervisordINIPath)
	if err != nil {
		return fmt.Errorf("create supervisord.ini failed, error: %s\n", err.Error())
	}
	defer f.Close()

	if err := tmpl.Execute(f, sConf); err != nil {
		return fmt.Errorf("file the supervisord.ini failed, error: %s\n", err.Error())
	}

	return nil
}

func Start() {
	appfile, err := os.ReadFile("conf/app.yml")
	if err != nil {
		log.Fatalf("read app.yml failed, error: %s\n", err.Error())
	}
	cvmApp := new(CvmApp)
	err = yaml.Unmarshal(appfile, &cvmApp)
	if err != nil {
		log.Fatalf("unmarshal app.yml failed, error: %s\n", err.Error())
	}
	time.Sleep(5 * time.Second)

	log.Println("do all the job over")

	startTask(cvmApp.CvmAssistants)

	startTask(cvmApp.AppInfo)
}

func startTask(tasks []*TaskInfo) {
	for i, t := range tasks {
		switch t.Type {
		case JOB:
			log.Printf("begin to do job %s\n", t.Name)
			err := DoJob(t)
			if err != nil {
				log.Fatalf("do job %s failed, error: %s\n", t.Name, err.Error())
			}
			log.Printf("end to do job %s\n", t.Name)
		case SERVER:
			log.Printf("begin to deploy server %s\n", t.Name)
			t.Priority = i + 2
			err := CreateSevers(t)
			if err != nil {
				log.Fatalf("deploy server %s failed, error: %s\n", t.Name, err)
			}
			err = RunCommand("supervisorctl", nil, "update")
			if err != nil {
				log.Fatalf("update supervisor conf failed, error: %s", err.Error())
			}
			err = RunCommand("supervisorctl", nil, "start", t.Name)
			if err != nil {
				log.Fatalf("start %s failed, error: %s", t.Name, err.Error())
			}
			log.Printf("end to deply server %s\n", t.Name)
		case DockerApp:
			log.Printf("begin to run docker app %s\n", t.Name)
			log.Printf("docker app will only run the first app\n")
			ExecvDockerApp(t)
		default:
			log.Fatalf("task type: %s does not support", t.Type)
		}
	}
}
