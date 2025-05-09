package pdf

import "github.com/signintech/gopdf"

func SplitPdf(srcPdf, dstPdf string, pageno int) error {
	pdf := gopdf.GoPdf{}
	pdf.Start(gopdf.Config{PageSize: gopdf.Rect{W: 595.28, H: 841.89}})
	pdf.AddPage()
	tpl1 := pdf.ImportPage(srcPdf, pageno, "/MediaBox")
	pdf.UseImportedTemplate(tpl1, 0, 0, 595.28, 841.89)
	err := pdf.WritePdf(dstPdf)
	if err != nil {
		return err
	}
	return nil
}
