package cvm

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
