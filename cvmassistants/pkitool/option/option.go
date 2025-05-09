package option

var Config *PKIServerInfo

func init() {
	Config = NewConfig()
}

// NewAPIServer new server
func NewConfig() *PKIServerInfo {
	return &PKIServerInfo{}
}

type PKIServerInfo struct {
	ConfPath       string
	ServerCertPath string
	CaPath         string
	Port           string
}
