use std::array::TryFromSliceError;
use std::io;
use thiserror::Error;

#[derive(Error, Debug)]
pub enum QuoteGeneratorError {
    #[error("Invalid usage: expected exactly 1 argument (report data), received {actual}")]
    InvalidUsage { actual: usize },

    #[error("Report data must be at most {max} bytes, got {actual} bytes")]
    ReportDataTooLarge { max: usize, actual: usize },

    #[error("Failed to convert report data: {0}")]
    ReportDataConversion(#[from] TryFromSliceError),

    #[error("Failed to get TDX report")]
    TdxReportFailed,

    #[error("Failed to get TDX quote")]
    TdxQuoteFailed,

    #[error("TDX quote generation returned no data")]
    TdxQuoteEmpty,

    #[error("Failed to write quote file: {0}")]
    WriteQuoteFile(#[from] io::Error),
}
