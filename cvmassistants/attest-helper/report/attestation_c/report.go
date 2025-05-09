package attestation_c

/*
#cgo CFLAGS: -I/opt/gmssl/include/ -O0
#cgo LDFLAGS: -L/opt/gmssl/lib -Wl,-rpath=/opt/gmssl/lib -lcrypto -ldl
#include "attestation.h"
*/
import "C"
import (
	"bytes"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"os"
	"strconv"
	"unsafe"

	"github.com/sirupsen/logrus"
)

const SHA_LEN = 32
const CERT_ECC_MAX_SIG_SIZE = 72
const GUEST_ATTESTATION_NONCE_SIZE = 16
const GUEST_ATTESTATION_DATA_SIZE = 64
const VM_ID_SIZE = 16
const VM_VERSION_SIZE = 16
const SN_LEN = 64
const USER_DATA_SIZE = 64
const HASH_BLOCK_LEN = 32

const CSV_CERT_RSVD3_SIZE = 624
const CSV_CERT_RSVD4_SIZE = 368
const CSV_CERT_RSVD5_SIZE = 368
const HIGON_USER_ID_SIZE = 256
const SIZE_INT32 = 4
const ECC_POINT_SIZE = 72
const CHIP_KEY_ID_LEN = 16

type Hash_block_t struct {
	Block [HASH_BLOCK_LEN]byte
}

type chip_key_id_t struct {
	ID [CHIP_KEY_ID_LEN]byte
}

type ecc_pubkey_t struct {
	CurveID uint32
	Qx      [ECC_POINT_SIZE / SIZE_INT32]uint32
	Qy      [ECC_POINT_SIZE / SIZE_INT32]uint32
	UserID  [HIGON_USER_ID_SIZE / SIZE_INT32]uint32
}

type ecc_signature_t struct {
	SigR [ECC_POINT_SIZE / SIZE_INT32]uint32
	SigS [ECC_POINT_SIZE / SIZE_INT32]uint32
}

type Higon_csv_cert struct {
	Version     uint32
	APIMajor    byte
	APIMinor    byte
	Reserved1   byte
	Reserved2   byte
	PubkeyUsage uint32
	PubkeyAlgo  uint32
	Pubkey      [SIZE_INT32 + ECC_POINT_SIZE*2 + HIGON_USER_ID_SIZE]byte
	Reserved3   [CSV_CERT_RSVD3_SIZE]byte
	Sig1Usage   uint32
	Sig1Algo    uint32
	Sig1        [ECC_POINT_SIZE * 2]byte
	Reserved4   [CSV_CERT_RSVD4_SIZE]byte
	Sig2Usage   uint32
	Sig2Algo    uint32
	Sig2        [ECC_POINT_SIZE * 2]byte
	Reserved5   [CSV_CERT_RSVD5_SIZE]byte
}

type CSV_CERT_t = Higon_csv_cert

type csv_cert_chain_t struct {
	PEKCert CSV_CERT_t
	OCACert CSV_CERT_t
	CEKCert CSV_CERT_t
}

type CsvAttestationReport struct {
	UserPubkeyDigest Hash_block_t
	VMID             [VM_ID_SIZE]byte
	VMVersion        [VM_VERSION_SIZE]byte
	UserData         [USER_DATA_SIZE]byte
	MNonce           [GUEST_ATTESTATION_NONCE_SIZE]byte
	Measure          Hash_block_t
	Policy           uint32
	SigUsage         uint32
	SigAlgo          uint32
	ANonce           uint32
	Sig              [ECC_POINT_SIZE * 2]byte
	PEKCert          CSV_CERT_t
	SN               [SN_LEN]byte
	Reserved2        [32]byte
	Mac              Hash_block_t
}

type ReportDetailInfo struct {
	UserData   string `json:"userData"`
	Monce      string `json:"monce"`
	Measure    string `json:"measure"`
	VMId       string `json:"vmId"`
	VMVersion  string `json:"vmVersion"`
	ChipId     string `json:"chipId"`
	FullReport string `json:"fullReport"`
}

func GetReport(userdata string, sealingkey bool) (*CsvAttestationReport, error) {
	if len(userdata) > 64 {
		return nil, fmt.Errorf("user data is limit to 64 byte")
	}
	var GoCsvAttestationReport CsvAttestationReport
	//open debug log when get report
	C.app_log_level = 0
	// KVM_HC_VM_ATTESTATIONhoskerne，>5.10100，5.112
	kvm_hc_vm_attestation_string := os.Getenv("KVM_HC_VM_ATTESTATION")
	var kvm_hc_vm_attestation uint64
	if kvm_hc_vm_attestation_string == "" {
		kvm_hc_vm_attestation = 100
	} else {
		k, err := strconv.Atoi(kvm_hc_vm_attestation_string)
		if err != nil {
			return nil, fmt.Errorf("Convert KVM_HC_VM_ATTESTATION failed, it must be 100 or 12")
		}

		if k != 100 && k != 12 {
			return nil, fmt.Errorf("KVM_HC_VM_ATTESTATION must be 100 or 12, while input is %d", kvm_hc_vm_attestation)
		}
		kvm_hc_vm_attestation = uint64(k)
	}
	logrus.Debug("KVM_HC_VM_ATTESTATION is ", kvm_hc_vm_attestation)
	CCsvAttestationReport := (*C.Csv_attestation_report)(unsafe.Pointer(&GoCsvAttestationReport))
	ret := C.get_attestation_report_use_vmmcall(CCsvAttestationReport, C.CString(userdata), C.uint(kvm_hc_vm_attestation))
	//ret := C.get_attestation_report_use_ioctl(CCsvAttestationReport, C.CString(userdata))
	if int(ret) != 0 {
		return nil, fmt.Errorf("get attestation report failed, error: %d", int(ret))
	}

	if !sealingkey {
		GoCsvAttestationReport.Reserved2 = [32]byte{} // clear sealing key
	}
	return &GoCsvAttestationReport, nil
}

func VerifyReport(report *CsvAttestationReport) error {
	if report == nil {
		return fmt.Errorf("point of report is nil")
	}

	//open debug log when verify report
	C.app_log_level = 0
	CCsvAttestationReport := (*C.Csv_attestation_report)(unsafe.Pointer(report))
	ret := C.full_verify_report(CCsvAttestationReport)

	if int(ret) != 0 {
		return fmt.Errorf("verify attestation report failed, error: %d", int(ret))
	}

	return nil
}

func GetReportDetailInfo(d *CsvAttestationReport) *ReportDetailInfo {
	rdi := new(ReportDetailInfo)

	buf, err := MarshalCsvAttestationReport(d)
	if err != nil {
		message := fmt.Sprintf("change report to binary failed: %s", err.Error())
		logrus.Error(message)
	}
	rdi.FullReport = base64.StdEncoding.EncodeToString(buf)

	var UserData [64]uint8
	j := unsafe.Sizeof(d.UserData) / unsafe.Sizeof(uint32(0))
	for i := 0; i < int(j); i++ {
		tmp := (*uint32)(unsafe.Pointer(&d.UserData[i*4]))
		*tmp ^= d.ANonce
		copy(UserData[i*4:], (*[4]uint8)(unsafe.Pointer(tmp))[:])
	}
	rdi.UserData = string(bytes.TrimRight(UserData[:], "\x00"))

	var measure [32]uint8
	j = unsafe.Sizeof(d.Measure) / unsafe.Sizeof(uint32(0))
	for i := 0; i < int(j); i++ {
		tmp := (*uint32)(unsafe.Pointer(&d.Measure.Block[i*4]))
		*tmp ^= d.ANonce
		copy(measure[i*4:], (*[4]uint8)(unsafe.Pointer(tmp))[:])
	}
	rdi.Measure = hex.EncodeToString(measure[:])

	var mnonce [16]uint8
	j = unsafe.Sizeof(d.MNonce) / unsafe.Sizeof(uint32(0))
	for i := 0; i < int(j); i++ {
		tmp := (*uint32)(unsafe.Pointer(&d.MNonce[i*4]))
		*tmp ^= d.ANonce
		copy(mnonce[i*4:], (*[4]uint8)(unsafe.Pointer(tmp))[:])
	}
	rdi.Monce = hex.EncodeToString(mnonce[:])

	var vmid [16]uint8
	j = unsafe.Sizeof(d.VMID) / unsafe.Sizeof(uint32(0))
	for i := 0; i < int(j); i++ {
		tmp := (*uint32)(unsafe.Pointer(&d.VMID[i*4]))
		*tmp ^= d.ANonce
		copy(vmid[i*4:], (*[4]uint8)(unsafe.Pointer(tmp))[:])
	}
	rdi.VMId = hex.EncodeToString(vmid[:])

	var vmversion [16]uint8
	j = unsafe.Sizeof(d.VMVersion) / unsafe.Sizeof(uint32(0))
	for i := 0; i < int(j); i++ {
		tmp := (*uint32)(unsafe.Pointer(&d.VMVersion[i*4]))
		*tmp ^= d.ANonce
		copy(vmversion[i*4:], (*[4]uint8)(unsafe.Pointer(tmp))[:])
	}
	rdi.VMVersion = hex.EncodeToString(vmversion[:])

	var chipID [64]uint8
	j = (uintptr(unsafe.Pointer(&d.Reserved2)) - uintptr(unsafe.Pointer(&d.SN))) / uintptr(unsafe.Sizeof(uint32(0)))
	for i := 0; i < int(j); i++ {
		chipID32 := (*uint32)(unsafe.Pointer(&d.SN[i*4]))
		*chipID32 ^= d.ANonce
		copy(chipID[i*4:], (*[4]uint8)(unsafe.Pointer(chipID32))[:])
	}
	rdi.ChipId = string(bytes.TrimRight(chipID[:], "\x00"))

	return rdi
}

func GetSealingKey() (sealingkey string, err error) {
	report, err := GetReport("get-sealing-key", true)
	if err != nil {
		message := fmt.Sprintf("get sealing key failed, error: %s", err.Error())
		logrus.Error(message)
		return "", err
	}

	return hex.EncodeToString(report.Reserved2[:]), nil
}

func MarshalCsvAttestationReport(d *CsvAttestationReport) ([]byte, error) {
	buf := new(bytes.Buffer)
	err := binary.Write(buf, binary.LittleEndian, d)
	if err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func UnmarshalCsvAttestationReport(data []byte) (*CsvAttestationReport, error) {
	buf := bytes.NewReader(data)
	d := new(CsvAttestationReport)
	err := binary.Read(buf, binary.LittleEndian, d)
	if err != nil {
		return nil, err
	}
	return d, nil
}
