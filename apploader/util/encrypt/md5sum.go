package encrypt

import (
	"crypto/md5"
	"encoding/hex"
	"fmt"
	"github.com/pkg/errors"
	"io"
	"os"
)

func GetFileMd5Sum(filename string) (md5sum string, err error) {
	// full path
	path := fmt.Sprintf("./%s", filename)
	pFile, err := os.Open(path)
	if err != nil {
		errmes := fmt.Sprintf("open file failed，filename=%v, err=%v", filename, err)
		return "", errors.New(errmes)
	}
	defer pFile.Close()
	md5h := md5.New()
	io.Copy(md5h, pFile)
	return hex.EncodeToString(md5h.Sum(nil)), nil
}
