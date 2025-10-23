package cvm

// CvmBootSequence is the main application configuration
type CvmBootSequence struct {
	Kind          string      `yaml:"kind"`
	AppInfo       []*TaskInfo `yaml:"app"`
	CvmAssistants []*TaskInfo `yaml:"csvAssistants"`
}

// TaskInfo is the information of a task
type TaskInfo struct {
	Name       string      `yaml:"name"`
	Type       string      `yaml:"type"`
	Entrypoint string      `yaml:"entrypoint"`
	Env        interface{} `yaml:"env"`
	Priority   int         `yaml:"-"`
	Args       []string    `yaml:"args"`
}

// SupervisorConf is the configuration of a supervisor
type SupervisorConf struct {
	Name        string
	Command     string
	Workplace   string
	Environment string
	Priority    int
}
