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

type CvmBootManager interface {
	Start()
}

type cvmBootManager struct {
	config          *config.CvmConfig
	cvmBootSequence *CvmBootSequence
	secretService   secret.SecretService
}

// NewCvmBootManager creates a new cvm service
func NewCvmBootManager(config *config.CvmConfig, secretService secret.SecretService) (CvmBootManager, error) {
	service := &cvmBootManager{config: config, secretService: secretService}
	cvmBootSequence, err := service.loadConfig()
	if err != nil {
		return nil, err
	}
	service.cvmBootSequence = cvmBootSequence
	return service, nil
}

// Start starts the cvm service
func (s *cvmBootManager) Start() {
	s.processTasks(s.cvmBootSequence.CvmAssistants)
	s.processTasks(s.cvmBootSequence.AppInfo)
}

// loadConfig loads the cvm app config
func (cbm *cvmBootManager) loadConfig() (*CvmBootSequence, error) {
	appfile, err := os.ReadFile(cbm.config.ConfigPath)
	if err != nil {
		return nil, fmt.Errorf("read %s failed, error: %s", cbm.config.ConfigPath, err.Error())
	}
	cvmBootSequence := new(CvmBootSequence)
	err = yaml.Unmarshal(appfile, &cvmBootSequence)
	if err != nil {
		return nil, fmt.Errorf("unmarshal %s failed, error: %s", cbm.config.ConfigPath, err.Error())
	}
	// validate cvmBootSequence, we want no more than one app
	if len(cvmBootSequence.AppInfo) > 1 {
		return nil, fmt.Errorf("only one application is supported, but got %d", len(cvmBootSequence.AppInfo))
	}
	return cvmBootSequence, nil
}

func (cbm *cvmBootManager) executeTask(taskInfo *TaskInfo) error {
	if taskInfo.Type != JOB {
		return fmt.Errorf("this task is not a job")
	}

	envs := make([]string, 0)
	for k, v := range cbm.secretService.GetAllSecrets() {
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

	return command.RunCommand(taskInfo.Entrypoint, envs, taskInfo.Args...)

}

func (cbm *cvmBootManager) deployService(taskInfo *TaskInfo) error {
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

	tmpl, err := template.ParseFiles(cbm.config.SupervisorTemplatePath)
	if err != nil {
		return fmt.Errorf("parse %s failed, error: %s", cbm.config.SupervisorTemplatePath, err.Error())
	}

	supervisordINIPath := path.Join(cbm.config.SupervisorPath, fmt.Sprintf("%s.ini", taskInfo.Name))
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

func (cbm *cvmBootManager) processTasks(tasks []*TaskInfo) {
	for i, t := range tasks {
		switch t.Type {
		case JOB:
			log.Printf("Executing job: %s (entrypoint: %s)", t.Name, t.Entrypoint)
			err := cbm.executeTask(t)
			if err != nil {
				log.Fatalf("Failed to execute job %s: %v", t.Name, err)
			}
			log.Printf("Job completed: %s", t.Name)
		case SERVER:
			log.Printf("Deploying service: %s (entrypoint: %s)", t.Name, t.Entrypoint)
			t.Priority = i + 2
			err := cbm.deployService(t)
			if err != nil {
				log.Fatalf("Failed to deploy service %s: %v", t.Name, err)
			}
			log.Printf("Service deployed: %s", t.Name)
			log.Printf("Updating supervisor configuration for service: %s", t.Name)
			err = command.RunCommand("supervisorctl", nil, "update")
			if err != nil {
				log.Fatalf("Failed to update supervisor configuration: %v", err)
			}
			log.Printf("Starting service: %s with supervisor configuration", t.Name)
			err = command.RunCommand("supervisorctl", nil, "start", t.Name)
			if err != nil {
				log.Fatalf("Failed to start service %s: %v", t.Name, err)
			}
			log.Printf("Service started: %s", t.Name)
		default:
			log.Fatalf("Task type: %s is not supported", t.Type)
		}
	}
}
