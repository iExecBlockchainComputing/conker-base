package controllers

import (
	"crypto/x509"
	"fmt"
	"github.com/beego/beego/v2/core/logs"
	"github.com/gin-gonic/gin"
	"os"
	"path"
	"path/filepath"
	"pkitool/certsmanager"
	"pkitool/common/consts"
	"pkitool/option"
	"pkitool/util/file"
)

type CertInfo struct {
	CaCertPath           string            `json:",omitempty"`
	ServerCertPath       string            `json:",omitempty"`
	ServerPrivateKeyPath string            `json:",omitempty"`
	ServerCertInfo       *x509.Certificate `json:",omitempty"`
	CaCertInfo           *x509.Certificate `json:",omitempty"`
}

type ServerCertController struct {
	DefaultController
}

func (uc ServerCertController) GenerateCert(c *gin.Context) {
	var req certsmanager.X509Info
	if err := c.BindJSON(&req); err != nil {
		message := fmt.Sprintf("get body error. %+v", err)
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE__JSON__UNMARSHAL_FAILED, message, "", c)
		logs.Error(errMes)
		return
	}

	if req.CommonName == "" {
		message := fmt.Sprintf("common Name is null")
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_CREATE_CERT_FAILED, message, "", c)
		logs.Error(errMes)
		return
	}

	certSavePath := path.Join(option.Config.ServerCertPath, req.CommonName)
	if file.IsDir(certSavePath) {
		message := fmt.Sprintf("cert which common name is %s is already existed", req.CommonName)
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_CREATE_CERT_FAILED, message, "", c)
		logs.Error(errMes)
		return
	}

	certificate, err := certsmanager.GenerateServerCerts(certSavePath, option.Config.CaPath, option.Config.ConfPath, &req)
	if err != nil {
		message := fmt.Sprintf("generate server certs failed, error: %s", err.Error())
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_CREATE_CERT_FAILED, message, "", c)
		logs.Error(errMes)
		return
	}

	mesg, baseRes := uc.HandleSuccess(200, "creat server certs successful", "", c)

	absCertSavePath, err := filepath.Abs(certSavePath)
	if err != nil {
		message := fmt.Sprintf("get cert save path failed, error: %s", err.Error())
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_CREATE_CERT_FAILED, message, "", c)
		logs.Error(errMes)
		return
	}
	res := DataRes{
		*baseRes,
		CertInfo{
			ServerCertPath:       path.Join(absCertSavePath, "server.crt"),
			ServerPrivateKeyPath: path.Join(absCertSavePath, "server.key"),
			ServerCertInfo:       certificate,
		},
	}

	c.JSON(200, res)
	logs.Info(mesg)
}

func (uc ServerCertController) GetCertList(c *gin.Context) {
	serverCertPaths, err := os.ReadDir(option.Config.ServerCertPath)
	if err != nil {
		message := fmt.Sprintf("read server cert path failed, error: %s", err.Error())
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_GET_CERTS_FAILED, message, "", c)
		logs.Error(errMes)
		return
	}

	absServerCertPath, err := filepath.Abs(option.Config.ServerCertPath)
	if err != nil {
		message := fmt.Sprintf("get abs server cert path failed, error: %s", err.Error())
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_GET_CERTS_FAILED, message, "", c)
		logs.Error(errMes)
		return
	}

	certInfos := make([]CertInfo, 0)
	for _, sp := range serverCertPaths {
		if sp.IsDir() {
			certPath := path.Join(absServerCertPath, sp.Name(), "server.crt")
			keyPath := path.Join(absServerCertPath, sp.Name(), "server.key")
			certInfo, err := certsmanager.LoadPair(certPath, keyPath)
			if err != nil {
				logs.Warning("the cert which common name is %s load failed, error: %s, skip load", sp.Name(), err.Error())
			} else {
				certInfos = append(certInfos, CertInfo{
					ServerCertPath:       certPath,
					ServerPrivateKeyPath: keyPath,
					ServerCertInfo:       certInfo,
				})
			}
		}
	}
	mesg, baseRes := uc.HandleSuccess(200, "get cert list successful", "", c)
	res := DataRes{
		*baseRes,
		certInfos,
	}

	c.JSON(200, res)
	logs.Info(mesg)
}

func (uc ServerCertController) DeleteCert(c *gin.Context) {
	commonName := c.Query("common")
	fmt.Println(commonName)
	certPath := path.Join(option.Config.ServerCertPath, commonName)
	if !file.IsDir(certPath) || commonName == "" {
		message := fmt.Sprintf("cert of %s not found", commonName)
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_DELETE_CERT_FAILED, message, "", c)
		logs.Error(errMes)
		return

	}
	err := os.RemoveAll(certPath)
	if err != nil {
		message := fmt.Sprintf("read server cert path failed, error: %s", err.Error())
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_DELETE_CERT_FAILED, message, "", c)
		logs.Error(errMes)
		return
	}
	mesg, baseRes := uc.HandleSuccess(200, "delete cert successful", "", c)

	c.JSON(200, baseRes)
	logs.Info(mesg)
}

func (uc ServerCertController) GetCaCert(c *gin.Context) {

	absCaCertPath, err := filepath.Abs(option.Config.CaPath)
	if err != nil {
		message := fmt.Sprintf("get abs ca cert path failed, error: %s", err.Error())
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_GET_CA_CERT_FAILED, message, "", c)
		logs.Error(errMes)
		return
	}
	certPath := path.Join(absCaCertPath, "ca.crt")
	keyPath := path.Join(absCaCertPath, "private.key")
	caCertInfo, err := certsmanager.LoadPair(certPath, keyPath)
	if err != nil {
		message := fmt.Sprintf("the ca cert load failed, error: %s", err.Error())
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_GET_CA_CERT_FAILED, message, "", c)
		logs.Error(errMes)
		return
	}
	certInfo := CertInfo{
		CaCertPath: certPath,
		CaCertInfo: caCertInfo,
	}
	mesg, baseRes := uc.HandleSuccess(200, "get ca cert successful", "", c)
	res := DataRes{
		*baseRes,
		certInfo,
	}

	c.JSON(200, res)
	logs.Info(mesg)
}
