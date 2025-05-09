package version

import (
	"fmt"
	"github.com/spf13/cobra"
)

var version string

// NewCommand returns a new cobra.Command for exec
func NewCommand() *cobra.Command {
	//var confPath string
	cmd := &cobra.Command{
		Use:                "version",
		Args:               cobra.ArbitraryArgs,
		FParseErrWhitelist: cobra.FParseErrWhitelist{UnknownFlags: true},
		Short:              "the version of pkitool",
		Long:               "the version of pkitool",
		Run: func(cmd *cobra.Command, args []string) {
			fmt.Println("pkitool's version is " + version)
		},
	}
	//cmd.Flags().StringVarP(&confPath, "conf", "c", "", "the configure path of nginx")
	return cmd
}
