package start

import (
	"fmt"
	"github.com/beego/beego/v2/core/logs"
	"github.com/spf13/cobra"
	"io/ioutil"
	"os"
	"path"
	"pkitool/certsmanager"
	"pkitool/option"
	"pkitool/routers"
	"pkitool/util/file"
)

// NewCommand returns a new cobra.Command for exec
func NewCommand() *cobra.Command {
	cmd := &cobra.Command{
		Use:                "start",
		Args:               cobra.ArbitraryArgs,
		FParseErrWhitelist: cobra.FParseErrWhitelist{UnknownFlags: true},
		Short:              "start pki server",
		Long:               "init a root ca and start pki server for generate certs",
		RunE: func(cmd *cobra.Command, args []string) error {
			return runE(option.Config, cmd, args)
		},
	}
	cmd.Flags().StringVarP(&option.Config.ConfPath, "conf", "c", "conf/cert-conf/", "the configure path of cert's basic info")
	cmd.Flags().StringVarP(&option.Config.ServerCertPath, "certpath", "o", "cert", "the path of saving server cert, default server")
	cmd.Flags().StringVarP(&option.Config.CaPath, "capath", "a", "ca", "the path of saving root ca, default justServer")
	cmd.Flags().StringVarP(&option.Config.Port, "port", "p", "8081", "the port of server")
	return cmd
}

func runE(flag *option.PKIServerInfo, cmd *cobra.Command, args []string) error {
	if err := InitRootCA(flag.CaPath, flag.ConfPath); err != nil {
		panic(err)
	}

	err := os.MkdirAll(option.Config.ServerCertPath, os.ModePerm)
	if err != nil {
		panic(err)
	}
	logs.Info("pki server is running on port: ", flag.Port)
	err = routers.InitRouters().Run(fmt.Sprintf(":%s", flag.Port))
	if err != nil {
		panic("start server failed, error: " + err.Error())
	}
	logs.Info("pki server is Running...")
	return nil
}

func InitRootCA(caPath, confPath string) error {
	if !file.Exists(path.Join(caPath, "existed")) {
		if err := certsmanager.GenerateCaCerts(caPath, confPath); err != nil {
			logs.Error("generate ca certs failed, error: %s", err.Error())
			return err
		}
		err := ioutil.WriteFile(path.Join(caPath, "existed"), []byte("existed"), os.ModePerm)
		if err != nil {
			os.RemoveAll(caPath)
			return fmt.Errorf("mark ca existed faield, error: %s", err.Error())
		}
	} else {
		logs.Info("root ca is already init, skip")
	}
	return nil
}
