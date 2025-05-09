package certsmanager

import (
	"crypto/ecdsa"
	"crypto/elliptic"
	"crypto/rand"
	"crypto/tls"
	"crypto/x509"
	"encoding/json"
	"encoding/pem"
	"errors"
	"fmt"
	"io/ioutil"
	"log"
	"math/big"
	"os"
	"path"
	"pkitool/util/file"
	"text/template"
	"time"

	"github.com/beego/beego/v2/core/logs"
)

type X509Info struct {
	Name        string
	CommonName  string
	DNSNames    string
	IPAddresses string
	Expiration  string
}

type PKIConf struct {
	CaPath      string
	PKIToolPath string
}

// ECC
func GenerateECCPrivateKey() (*ecdsa.PrivateKey, error) {
	key, err := ecdsa.GenerateKey(elliptic.P256(), rand.Reader)
	if err != nil {
		return nil, err
	}
	return key, nil
}

func SaveKeyToFileUsePEM(key *ecdsa.PrivateKey, savePath string) error {
	keyDer, err := x509.MarshalECPrivateKey(key)
	keyBlock := &pem.Block{
		Type:  "EC PRIVATE KEY",
		Bytes: keyDer,
	}

	keyData := pem.EncodeToMemory(keyBlock)

	if err = ioutil.WriteFile(savePath, keyData, 0644); err != nil {
		logs.Error("save the private key to %s failed, error: %s", savePath, err.Error())
		return err
	}
	return nil
}

func SaveCertToFileUsePEM(cert *x509.Certificate, savePath string) error {
	certBlock := &pem.Block{
		Type:  "CERTIFICATE",
		Bytes: cert.Raw,
	}

	pemData := pem.EncodeToMemory(certBlock)

	if err := ioutil.WriteFile(savePath, pemData, 0644); err != nil {
		logs.Error("save the server crt to %s, failed, error: %s", savePath, err.Error())
		return err
	}
	return nil
}

func CreateCertUseECC(csrConf string, privateKey *ecdsa.PrivateKey, isCa bool, parent *x509.Certificate, serverKey *ecdsa.PrivateKey) (*x509.Certificate, error) {
	var csr x509.Certificate

	csrByte, err := ioutil.ReadFile(csrConf)
	if err != nil {
		logs.Error("read csr conf from %s failed, error: %s", csrConf, err.Error())
		return nil, err
	}

	if err := json.Unmarshal(csrByte, &csr); err != nil {
		logs.Error("unmarshl csr conf failed, error: %s", err.Error())
		return nil, err
	}

	if isCa {
		csr.Version = 3
		csr.SerialNumber = big.NewInt(time.Now().Unix())
		csr.NotBefore = time.Now()
		csr.NotAfter = time.Now().AddDate(20, 0, 0)
		csr.BasicConstraintsValid = true
		csr.IsCA = true
		csr.MaxPathLen = 1
		csr.MaxPathLenZero = false
		csr.KeyUsage = x509.KeyUsageCertSign | x509.KeyUsageCRLSign
		parent = &csr
		serverKey = privateKey
	} else {
		csr.Version = 3
		csr.SerialNumber = big.NewInt(time.Now().Unix())
		csr.NotBefore = time.Now()
		csr.NotAfter = time.Now().AddDate(10, 0, 0)
		csr.BasicConstraintsValid = true
		csr.IsCA = false
		csr.KeyUsage = x509.KeyUsageDigitalSignature | x509.KeyUsageKeyEncipherment
		csr.ExtKeyUsage = []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth}
		if parent == nil {
			logs.Error("create server cert must provider parent cert")
			return nil, fmt.Errorf("create server cert must provider parent cert")
		}

		if serverKey == nil {
			logs.Error("create server cert must provider serverKey")
			return nil, fmt.Errorf("create server cert must provider serverKey")
		}

	}

	der, err := x509.CreateCertificate(rand.Reader, &csr, parent, serverKey.Public(), privateKey)
	if err != nil {
		logs.Error("create cert failed, error: %s\n", err.Error())
		return nil, err
	}

	cert, err := x509.ParseCertificate(der)
	if err != nil {
		log.Printf("parse cert faield, error: %s", err.Error())
		return nil, err
	}
	return cert, nil
}

func LoadPair(certFile, keyFile string) (cert *x509.Certificate, err error) {
	if len(certFile) == 0 && len(keyFile) == 0 {
		return nil, errors.New("cert or key has not provided")
	}
	// load cert and key by tls.LoadX509KeyPair
	tlsCert, err := tls.LoadX509KeyPair(certFile, keyFile)
	if err != nil {
		return
	}

	cert, err = x509.ParseCertificate(tlsCert.Certificate[0])
	return
}

func LoadAndCheckCerts(certsPath, caSavePath string) (caCert, serverCert *x509.Certificate, err error) {
	caCertPath := path.Join(caSavePath, "ca.crt")
	privateKeyPath := path.Join(certsPath, "private.key")
	serverCertPath := path.Join(certsPath, "server.crt")
	serverKeyPath := path.Join(certsPath, "server.key")

	caCert, err = LoadPair(caCertPath, privateKeyPath)
	if err != nil {
		logs.Error("load ca cert failed, error: %s", err.Error())
		return nil, nil, err
	}

	serverCert, err = LoadPair(serverCertPath, serverKeyPath)
	if err != nil {
		logs.Error("load server cert failed, error: %s", err.Error())
		return nil, nil, err
	}

	return caCert, serverCert, nil
}

func GenerateCaCerts(caSavePath, confPath string) error {
	var PrivateKey *ecdsa.PrivateKey
	var CaCert *x509.Certificate
	var err error

	if !file.IsDir(caSavePath) {
		if err := os.MkdirAll(caSavePath, os.ModePerm); err != nil {
			return err
		}
	}

	caCertPath := path.Join(caSavePath, "ca.crt")
	privateKeyPath := path.Join(caSavePath, "private.key")
	logs.Info("begin to generate new pair of ca and server certs, use csr confs in %s", confPath)
	if file.IsFile(caCertPath) && file.IsFile(privateKeyPath) {
		log.Printf("ca certs is already generate\n")
		return nil
	}

	PrivateKey, err = GenerateECCPrivateKey()
	if err != nil {
		return err
	}

	caConf := path.Join(confPath, "ca-csr.json")
	CaCert, err = CreateCertUseECC(caConf, PrivateKey, true, nil, nil)
	if err != nil {
		return err
	}

	if err := SaveCertToFileUsePEM(CaCert, caCertPath); err != nil {
		return err
	}
	logs.Info("ca.crt generate successful, save in %s", caCertPath)

	if err := SaveKeyToFileUsePEM(PrivateKey, privateKeyPath); err != nil {
		return err
	}
	logs.Info("private.key generate successful, save in %s", privateKeyPath)

	return nil
}

func GenerateServerCerts(savePath, caPath, confPath string, info *X509Info) (*x509.Certificate, error) {
	var PrivateKey *ecdsa.PrivateKey
	var CaCert *x509.Certificate
	var err error

	serverCsrTemplate := path.Join(confPath, "server-csr.json.template")
	tmpl, err := template.ParseFiles(serverCsrTemplate)
	if err != nil {
		return nil, fmt.Errorf("parse server-csr.json template file failed, error: %s", err.Error())
	}

	serverCsrPath := path.Join(confPath, fmt.Sprintf("%s-server-csr.json", info.Name))
	if file.IsFile(serverCsrPath) {
		os.RemoveAll(serverCsrPath)
	}

	f, err := os.Create(serverCsrPath)
	if err != nil {
		return nil, fmt.Errorf("create server-csr.json failed, error: %s", err.Error())
	}
	defer f.Close()

	info.DNSNames = info.CommonName
	info.IPAddresses = info.CommonName
	info.Name = "{{.Name}}"
	if err := tmpl.Execute(f, info); err != nil {
		return nil, fmt.Errorf("fill the server-csr.json failed, error: %s", err.Error())
	}

	caCertPath := path.Join(caPath, "ca.crt")
	privateKeyPath := path.Join(caPath, "private.key")

	if !file.IsDir(savePath) {
		if err := os.MkdirAll(savePath, os.ModePerm); err != nil {
			return nil, err
		}
	}
	serverCertPath := path.Join(savePath, "server.crt")
	serverKeyPath := path.Join(savePath, "server.key")
	logs.Info("begin to generate new pair of server certs, use csr confs in %s", confPath)

	if file.IsFile(caCertPath) != true {
		os.RemoveAll(savePath)
		return nil, fmt.Errorf("caCertsPath %s is not exist", caCertPath)
	}

	if file.IsFile(privateKeyPath) != true {
		os.RemoveAll(savePath)
		return nil, fmt.Errorf("privaete key savePath %s is not exist", privateKeyPath)
	}

	CaCert, err = LoadPair(caCertPath, privateKeyPath)
	if err != nil {
		os.RemoveAll(savePath)
		return nil, err
	}

	pbyte, err := ioutil.ReadFile(privateKeyPath)
	if err != nil {
		os.RemoveAll(savePath)
		return nil, err
	}
	der, _ := pem.Decode(pbyte)
	PrivateKey, err = x509.ParseECPrivateKey(der.Bytes)
	if err != nil {
		os.RemoveAll(savePath)
		return nil, err
	}

	serverKey, err := GenerateECCPrivateKey()
	if err != nil {
		os.RemoveAll(savePath)
		return nil, err
	}
	serverCert, err := CreateCertUseECC(serverCsrPath, PrivateKey, false, CaCert, serverKey)
	if err != nil {
		os.RemoveAll(savePath)
		return nil, err
	}
	if err := SaveCertToFileUsePEM(serverCert, serverCertPath); err != nil {
		os.RemoveAll(savePath)
		return nil, err
	}
	logs.Info("server.crt generate successful, save in %s", serverCertPath)

	if err := SaveKeyToFileUsePEM(serverKey, serverKeyPath); err != nil {
		os.RemoveAll(savePath)
		return nil, err
	}
	logs.Info("server.key generate successful, save in %s", serverKeyPath)

	return serverCert, nil
}
