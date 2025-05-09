package secret_server

import (
	"log"
	"net/http"

	"github.com/gin-gonic/gin"
)

type JsonResult struct {
	Code int    `json:"code"`
	Msg  string `json:"msg"`
}

var Secret map[string]string

func StartSecretServer() {
	Secret = make(map[string]string)
	gin.SetMode(gin.ReleaseMode)
	r := gin.Default()
	r.POST("/secret", func(c *gin.Context) {
		k := c.PostForm("key")
		v := c.PostForm("value") //

		Secret[k] = v
		c.JSON(http.StatusOK, gin.H{
			"code":    200,
			"message": "update secret successful",
		})
	})

	//r.GET("/secret", func(c *gin.Context) {
	//	k := c.PostForm("key")
	//
	//	v := Secret[k]
	//	fmt.Println(v)
	//	c.JSON(http.StatusOK, gin.H{
	//		"code": 200,
	//		"message": "update secret successful",
	//		"value":v,
	//	})
	//})
	log.Printf("secret server start successful")
	r.Run(":9090") // listen and serve on 0.0.0.0:8080
}
