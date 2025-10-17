package csvapp

/*
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
*/
import "C"
import (
	"apploader/pkg/conversion"
	"apploader/pkg/file"
	"apploader/secret_server"
	"fmt"
	"io/ioutil"
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

type CsvApp struct {
	Kind          string      `yaml:"kind"`
	AppInfo       []*TaskInfo `yaml:"app"`
	CsvAssistants []*TaskInfo `yaml:"csvAssistants"`
}

type TaskInfo struct {
	TLSInfo    *TLSInfo    `yaml:"tls"`
	Name       string      `yaml:"name"`
	Type       string      `yaml:"type"`
	Entrypoint string      `yaml:"entrypoint"`
	Env        interface{} `yaml:"env"`
	Priority   int         `yaml:"-"`
	Args       []string    `yaml:"args"`
}

type SupervisorConf struct {
	Name        string
	Command     string
	Workplace   string
	Environment string
	Priority    int
}

type TLSInfo struct {
	CertPath   string `yaml:"certPath"`
	CommonName string `yaml:"commonName"`
}

func DoJob(ca *TaskInfo) error {
	if ca.Type != JOB {
		return fmt.Errorf("this task is not a job")
	}

	envs := make([]string, 0)
	for k, v := range secret_server.Secret {
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

	if ca.TLSInfo != nil {
		encryptedDir := "/workplace/encryptedData"
		log.Printf("[warning] use PKI, please make sure %s is an encrypted disk", encryptedDir)
		caPath := path.Join(encryptedDir, "pki")
		if !file.Exists(path.Join(caPath, "existed")) {
			err := RunCommand("/workplace/csv-agent/csvassistants/pkitool/pkitool", nil, "cert", "-m", "ca", "-a", caPath, "-c", "/workplace/csv-agent/csvassistants/pkitool/conf/cert-conf")
			if err != nil {
				return fmt.Errorf("init pki ca failed, error: %s", err.Error())
			}
			err = ioutil.WriteFile(path.Join(caPath, "existed"), []byte("existed"), os.ModePerm)
			if err != nil {
				os.RemoveAll(caPath)
				return fmt.Errorf("mark ca existed faield, error: %s", err.Error())
			}
		}
		if !file.Exists(path.Join(ca.TLSInfo.CertPath, "existed")) {
			err := RunCommand("/workplace/csv-agent/csvassistants/pkitool/pkitool", nil, "cert", "-m", "server", "-a", caPath, "-o", ca.TLSInfo.CertPath, "-n", ca.TLSInfo.CommonName)
			if err != nil {
				return fmt.Errorf("create tls cert failed, error: %s\n", err.Error())
			}

			err = ioutil.WriteFile(path.Join(ca.TLSInfo.CertPath, "existed"), []byte("existed"), os.ModePerm)
			if err != nil {
				os.RemoveAll(ca.TLSInfo.CertPath)
				return fmt.Errorf("mark ca existed faield, error: %s", err.Error())
			}
		}

		caString, err := ioutil.ReadFile(path.Join(caPath, "ca.crt"))
		if err != nil {
			return fmt.Errorf("read ca.crt failed, error: %s", err.Error())
		}
		log.Println("the ca.crt is as follow")
		fmt.Printf("%s", caString)
		log.Println("*********************************")
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

	if ca.TLSInfo != nil {
		encryptedDir := "/workplace/encryptedData"
		log.Printf("[warning] use PKI, please make sure %s is an encrypted disk", encryptedDir)
		caPath := path.Join(encryptedDir, "pki")
		if !file.Exists(path.Join(caPath, "existed")) {
			err := RunCommand("/workplace/csv-agent/csvassistants/pkitool/pkitool", nil, "cert", "-m", "ca", "-a", caPath, "-c", "/workplace/csv-agent/csvassistants/pkitool/conf/cert-conf")
			if err != nil {
				log.Fatalf("init pki ca failed, error: %s", err.Error())
			}
			err = ioutil.WriteFile(path.Join(caPath, "existed"), []byte("existed"), os.ModePerm)
			if err != nil {
				os.RemoveAll(caPath)
				log.Fatalf("mark ca existed faield, error: %s", err.Error())
			}
		}
		if !file.Exists(path.Join(ca.TLSInfo.CertPath, "existed")) {
			err := RunCommand("/workplace/csv-agent/csvassistants/pkitool/pkitool", nil, "cert", "-m", "server", "-a", caPath, "-o", ca.TLSInfo.CertPath, "-n", ca.TLSInfo.CommonName)
			if err != nil {
				log.Fatalf("create tls cert failed, error: %s\n", err.Error())
			}

			err = ioutil.WriteFile(path.Join(ca.TLSInfo.CertPath, "existed"), []byte("existed"), os.ModePerm)
			if err != nil {
				os.RemoveAll(ca.TLSInfo.CertPath)
				log.Fatalf("mark ca existed faield, error: %s", err.Error())
			}
		}
	}

	for k, v := range secret_server.Secret {
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
	if ca.TLSInfo != nil {
		encryptedDir := "/workplace/encryptedData"
		log.Printf("[warning] use PKI, please make sure %s is an encrypted disk", encryptedDir)
		caPath := path.Join(encryptedDir, "pki")
		if !file.Exists(path.Join(caPath, "existed")) {
			err := RunCommand("/workplace/csv-agent/csvassistants/pkitool/pkitool", nil, "cert", "-m", "ca", "-a", caPath, "-c", "/workplace/csv-agent/csvassistants/pkitool/conf/cert-conf")
			if err != nil {
				return fmt.Errorf("init pki ca failed, error: %s", err.Error())
			}
			err = ioutil.WriteFile(path.Join(caPath, "existed"), []byte("existed"), os.ModePerm)
			if err != nil {
				os.RemoveAll(caPath)
				return fmt.Errorf("mark ca existed faield, error: %s", err.Error())
			}
		}
		if !file.Exists(path.Join(ca.TLSInfo.CertPath, "existed")) {
			err := RunCommand("/workplace/csv-agent/csvassistants/pkitool/pkitool", nil, "cert", "-m", "server", "-a", caPath, "-o", ca.TLSInfo.CertPath, "-n", ca.TLSInfo.CommonName)
			if err != nil {
				return fmt.Errorf("create tls cert failed, error: %s\n", err.Error())
			}

			err = ioutil.WriteFile(path.Join(ca.TLSInfo.CertPath, "existed"), []byte("existed"), os.ModePerm)
			if err != nil {
				os.RemoveAll(ca.TLSInfo.CertPath)
				return fmt.Errorf("mark ca existed faield, error: %s", err.Error())
			}
		}

		caString, err := ioutil.ReadFile(path.Join(caPath, "ca.crt"))
		if err != nil {
			return fmt.Errorf("read ca.crt failed, error: %s", err.Error())
		}
		log.Println("the ca.crt is as follow")
		fmt.Printf("%s", caString)
		log.Println("*********************************")
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
	appfile, err := ioutil.ReadFile("conf/app.yml")
	if err != nil {
		log.Fatalf("read app.yml failed, error: %s\n", err.Error())
	}
	csvApp := new(CsvApp)
	err = yaml.Unmarshal(appfile, &csvApp)
	if err != nil {
		log.Fatalf("unmarshal app.yml failed, error: %s\n", err.Error())
	}
	time.Sleep(5 * time.Second)

	log.Println("do all the job over")

	startTask(csvApp.CsvAssistants)

	startTask(csvApp.AppInfo)
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
