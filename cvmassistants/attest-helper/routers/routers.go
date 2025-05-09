package routers

import (
	"attest-helper/controllers"
	"github.com/gin-gonic/gin"
)

func InitRouters() *gin.Engine {
	gin.SetMode(gin.ReleaseMode)
	router := gin.Default()

	attest := router.Group("/v1/attest")

	attestationController := controllers.AttestationController{}
	attest.POST("/report", attestationController.GetReport)
	attest.POST("/verify", attestationController.VerifyReport)
	attest.GET("/sealingkey",attestationController.GetSealingKey)

	return router
}
