package image

import "github.com/h2non/bimg"

func Pdf2Image(pdfPath, savePath string) error {
	buffer, err := bimg.Read(pdfPath)
	if err != nil {
		return err
	}

	newImage, err := bimg.NewImage(buffer).Convert(bimg.JPEG)
	if err != nil {
		return err
	}
	err = bimg.Write(savePath, newImage)
	if err != nil {
		return err
	}
	return nil

}
