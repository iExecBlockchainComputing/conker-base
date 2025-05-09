package routers

import (
	"github.com/gin-gonic/gin"
	"pkitool/controllers"
)

func InitRouters() *gin.Engine {
	gin.SetMode(gin.ReleaseMode)
	router := gin.Default()

	cert := router.Group("/v1/cert")

	serverCertController := controllers.ServerCertController{}
	cert.POST("/server", serverCertController.GenerateCert)
	cert.GET("/server", serverCertController.GetCertList)
	cert.DELETE("/server", serverCertController.DeleteCert)
	cert.GET("/ca", serverCertController.GetCaCert)

	return router
}
