package certs

import (
	"github.com/beego/beego/v2/core/logs"
	"github.com/spf13/cobra"
	"pkitool/certsmanager"
)

type CertInfo struct {
	Mode     string
	ConfPath string
	CertPath string
	CaPath   string
	X509Info certsmanager.X509Info
}

// NewCommand returns a new cobra.Command for exec
func NewCommand() *cobra.Command {
	flags := &CertInfo{}
	cmd := &cobra.Command{
		Use:   "cert [flags]",
		Short: "generate ca and server cert and private key",
		Long:  "generate ca and server cert and private key",
		RunE: func(cmd *cobra.Command, args []string) error {
			return runE(flags, cmd, args)
		},
	}
	cmd.Flags().StringVarP(&flags.ConfPath, "conf", "c", "conf/cert-conf/", "the configure path of cert's basic info")
	cmd.Flags().StringVarP(&flags.Mode, "mode", "m", "server", "[server]:just generate a pair of server's certs \n [ca] generate ca certs, default justServer")
	cmd.Flags().StringVarP(&flags.CertPath, "certpath", "o", "cert", "the path of saving server cert, default server")
	cmd.Flags().StringVarP(&flags.CaPath, "capath", "a", "ca", "the path of saving root ca, default justServer")
	cmd.Flags().StringVarP(&flags.X509Info.CommonName, "commonName", "n", "127.0.0.1", "the commonName of the server cert")
	return cmd
}

func runE(flags *CertInfo, cmd *cobra.Command, args []string) error {
	if flags.Mode == "ca" {
		if err := certsmanager.GenerateCaCerts(flags.CaPath, flags.ConfPath); err != nil {
			logs.Error("generate ca certs failed, error: %s", err.Error())
			return err
		}
	} else {
		if _, err := certsmanager.GenerateServerCerts(flags.CertPath, flags.CaPath, flags.ConfPath, &flags.X509Info); err != nil {
			logs.Error("generate server certs failed, error: %s", err.Error())
			return err
		}
	}

	logs.Info("generate certs over")
	return nil
}
