package controllers

import (
	"attest-helper/consts"
	"attest-helper/report/attestation_c"
	"encoding/base64"
	"fmt"
	"github.com/gin-gonic/gin"
	"github.com/sirupsen/logrus"
)

type ReportRes struct {
	BaseRes
	ReportData *attestation_c.ReportDetailInfo `json:"reportData"`
}

type SealingKeyRes struct {
	BaseRes
	SealingKey string `json:"sealingKey"`
}

type AttestationController struct {
	DefaultController
}

func (uc AttestationController) GetReport(c *gin.Context) {
	var req attestation_c.ReportDetailInfo
	if err := c.BindJSON(&req); err != nil {
		message := fmt.Sprintf("get body error. %+v", err)
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE__JSON__UNMARSHAL_FAILED, message, "", c)
		logrus.Error(errMes)
		return
	}

	report, err := attestation_c.GetReport(req.UserData, false)
	if err != nil {
		message := fmt.Sprintf("get report failed, error: %s", err.Error())
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_GET_REPORT_FAILED, message, "", c)
		logrus.Error(errMes)
		return
	}

	reportDetail := attestation_c.GetReportDetailInfo(report)

	mesg, baseRes := uc.HandleSuccess(200, "get report successful", "", c)

	res := ReportRes{
		*baseRes,
		reportDetail,
	}

	c.JSON(200, res)
	logrus.Info(mesg)
}

func (uc AttestationController) VerifyReport(c *gin.Context) {
	var req attestation_c.ReportDetailInfo
	if err := c.BindJSON(&req); err != nil {
		message := fmt.Sprintf("get body error. %+v", err)
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE__JSON__UNMARSHAL_FAILED, message, "", c)
		logrus.Error(errMes)
		return
	}

	reportByte, err := base64.StdEncoding.DecodeString(req.FullReport)
	if err != nil {
		message := fmt.Sprintf("decode report failed, error: %s", err.Error())
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_VERIFY_REPORT_FAILED, message, "", c)
		logrus.Error(errMes)
		return
	}

	report, err := attestation_c.UnmarshalCsvAttestationReport(reportByte)
	if err != nil {
		message := fmt.Sprintf("unmarsh report info failed, error: %s", err.Error())
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_VERIFY_REPORT_FAILED, message, "", c)
		logrus.Error(errMes)
		return
	}

	err = attestation_c.VerifyReport(report)
	if err != nil {
		message := fmt.Sprintf("verify report failed, error: %s", err.Error())
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_GET_REPORT_FAILED, message, "", c)
		logrus.Error(errMes)
		return
	}

	reportDetail := attestation_c.GetReportDetailInfo(report)

	mesg, baseRes := uc.HandleSuccess(200, "verify report successful", "", c)

	res := ReportRes{
		*baseRes,
		reportDetail,
	}

	c.JSON(200, res)
	logrus.Info(mesg)
}

func (uc AttestationController) GetSealingKey(c *gin.Context) {
	sealingkey, err := attestation_c.GetSealingKey()
	if err != nil {
		message := fmt.Sprintf("get sealing key failed, error: %s", err.Error())
		errMes := uc.HandleErrorStatusBadRequest(consts.ERROR_CODE_GET_REPORT_FAILED, message, "", c)
		logrus.Error(errMes)
		return
	}

	mesg, baseRes := uc.HandleSuccess(200, "get sealing key successful", "", c)

	res := SealingKeyRes{
		*baseRes,
		sealingkey,
	}
	c.JSON(200, res)
	logrus.Info(mesg)
}
