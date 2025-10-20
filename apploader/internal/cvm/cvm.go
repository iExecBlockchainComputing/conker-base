package cvm

import (
	"apploader/internal/config"
	"apploader/internal/secret"
	"apploader/pkg/command"
	"apploader/pkg/conversion"
	"apploader/pkg/file"
	"fmt"
	"log"
	"os"
	"path"
	"strings"
	"text/template"

	"gopkg.in/yaml.v3"
)

const (
	JOB    = "job"
	SERVER = "server"
)

type CvmService interface {
	Start()
}

type cvmService struct {
	config *config.CvmConfig
	cvmApp *CvmApp
}

// NewCvmService creates a new cvm service
func NewCvmService(config *config.CvmConfig) (CvmService, error) {
	service := &cvmService{config: config}
	cvmApp, err := service.loadConfig()
	if err != nil {
		return nil, err
	}
	service.cvmApp = cvmApp
	return service, nil
}

// Start starts the cvm service
func (s *cvmService) Start() {
	s.startTask(s.cvmApp.CvmAssistants)
	s.startTask(s.cvmApp.AppInfo)
}

// loadConfig loads the cvm app config
func (s *cvmService) loadConfig() (*CvmApp, error) {
	appfile, err := os.ReadFile(s.config.ConfigPath)
	if err != nil {
		return nil, fmt.Errorf("read %s failed, error: %s", s.config.ConfigPath, err.Error())
	}
	cvmApp := new(CvmApp)
	err = yaml.Unmarshal(appfile, &cvmApp)
	if err != nil {
		return nil, fmt.Errorf("unmarshal %s failed, error: %s", s.config.ConfigPath, err.Error())
	}
	return cvmApp, nil
}

func (s *cvmService) DoJob(taskInfo *TaskInfo) error {
	if taskInfo.Type != JOB {
		return fmt.Errorf("this task is not a job")
	}

	envs := make([]string, 0)
	for k, v := range secret.Secret {
		envs = append(envs, fmt.Sprintf("%s=%s", k, v))
	}

	if taskInfo.Env != nil {
		userEnv, ok := taskInfo.Env.(map[string]interface{})
		if !ok {
			return fmt.Errorf("user env format error")
		}

		for k, v := range userEnv {
			envs = append(envs, fmt.Sprintf("%s=%s", k, v))
		}
	}

	log.Printf("entrypoint is %s", taskInfo.Entrypoint)
	return command.RunCommand(taskInfo.Entrypoint, envs, taskInfo.Args...)

}

func (s *cvmService) CreateSevers(taskInfo *TaskInfo) error {
	if taskInfo.Type != SERVER {
		return fmt.Errorf("task is not a server")
	}

	envs := make([]string, 0)

	// fixme: envs in supervisord will write to file of supervisord.ini, it will expose the secret envs, so deleted
	//for k, v := range secret_server.Secret {
	//	envs = append(envs, fmt.Sprintf("%s=%s", k, v))
	//}

	if taskInfo.Env != nil {
		userEnv, ok := taskInfo.Env.(map[string]interface{})
		if !ok {
			return fmt.Errorf("user env format error")
		}
		for k, v := range userEnv {
			value := conversion.Interface2String(v)
			envs = append(envs, fmt.Sprintf("%s=%s", k, value))
		}
	}

	sConf := new(SupervisorConf)
	sConf.Name = taskInfo.Name
	sConf.Command = taskInfo.Entrypoint + " " + strings.Join(taskInfo.Args, " ")
	sConf.Workplace = path.Dir(taskInfo.Entrypoint)
	sConf.Environment = strings.Join(envs, ",")
	sConf.Priority = taskInfo.Priority

	tmpl, err := template.ParseFiles(s.config.SupervisorTemplatePath)
	if err != nil {
		return fmt.Errorf("parse %s failed, error: %s", s.config.SupervisorTemplatePath, err.Error())
	}

	supervisordINIPath := path.Join(s.config.SupervisorPath, fmt.Sprintf("%s.ini", taskInfo.Name))
	if file.IsFile(supervisordINIPath) {
		os.RemoveAll(supervisordINIPath)
	}

	f, err := os.Create(supervisordINIPath)
	if err != nil {
		return fmt.Errorf("create %s failed, error: %s", supervisordINIPath, err.Error())
	}
	defer f.Close()

	if err := tmpl.Execute(f, sConf); err != nil {
		return fmt.Errorf("fill the %s failed, error: %s", supervisordINIPath, err.Error())
	}

	return nil
}

func (s *cvmService) startTask(tasks []*TaskInfo) {
	for i, t := range tasks {
		switch t.Type {
		case JOB:
			log.Printf("begin to do job %s\n", t.Name)
			err := s.DoJob(t)
			if err != nil {
				log.Fatalf("do job %s failed, error: %s\n", t.Name, err.Error())
			}
			log.Printf("end to do job %s\n", t.Name)
		case SERVER:
			log.Printf("begin to deploy server %s\n", t.Name)
			t.Priority = i + 2
			err := s.CreateSevers(t)
			if err != nil {
				log.Fatalf("deploy server %s failed, error: %s\n", t.Name, err)
			}
			err = command.RunCommand("supervisorctl", nil, "update")
			if err != nil {
				log.Fatalf("update supervisor conf failed, error: %s", err.Error())
			}
			err = command.RunCommand("supervisorctl", nil, "start", t.Name)
			if err != nil {
				log.Fatalf("start %s failed, error: %s", t.Name, err.Error())
			}
			log.Printf("end to deply server %s\n", t.Name)
		default:
			log.Fatalf("task type: %s does not support", t.Type)
		}
	}
}
