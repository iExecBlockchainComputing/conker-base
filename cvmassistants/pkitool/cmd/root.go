package cmd

import (
	"github.com/beego/beego/v2/core/logs"
	"github.com/spf13/cobra"
	"os"
	"pkitool/cmd/certs"
	"pkitool/cmd/start"
	"pkitool/cmd/version"
	"pkitool/common/consts"
)

// NewCommand returns a new cobra.Command implementing the root command for kinder
func NewCommand() *cobra.Command {
	cmd := &cobra.Command{
		Args:         cobra.NoArgs,
		Use:          "pkitool",
		Short:        "pkitool is a tool for generate certs",
		Long:         "",
		SilenceUsage: true,
	}
	InitLog()
	// add kind commands customized in kind
	cmd.AddCommand(certs.NewCommand())
	cmd.AddCommand(version.NewCommand())
	cmd.AddCommand(start.NewCommand())
	return cmd
}

// Run runs the `kind` root command
func Run() error {
	return NewCommand().Execute()
}

func InitLog() {
	if err := os.MkdirAll(consts.LogDir, os.ModePerm); err != nil {
		panic(err)
	}
	if err := logs.SetLogger(logs.AdapterFile, `{"maxdays":0, "filename": "logs/pki.log"}`); err != nil {
		panic(err)
	}
	logs.SetLogger("console")
	logs.SetLevel(6)
	logs.EnableFuncCallDepth(true)

}
