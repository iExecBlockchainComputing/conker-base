use crate::error::QuoteGeneratorError;
use log::error;

const REPORT_SIZE: usize = 1024;
const TDX_UUID_SIZE: usize = 16;

/// Creates a TDX report data structure from input bytes.
///
/// # Arguments
///
/// * `input_bytes` - A byte slice that **must be exactly `REPORT_DATA_SIZE` bytes long**.
///   In this binary, `main` guarantees this by copying/padding the user input into
///   a fixed-size `REPORT_DATA_SIZE` buffer before calling this function.
///
/// # Returns
///
/// A `Result` containing the `tdx_report_data_t` structure, or a `QuoteGeneratorError`
/// if the input bytes cannot be converted.
///
/// # Errors
///
/// Returns `QuoteGeneratorError::ReportDataConversion` if input bytes length doesn't match `REPORT_DATA_SIZE`.
pub fn create_report_data(
    input_bytes: &[u8],
) -> Result<tdx_attest_rs::tdx_report_data_t, QuoteGeneratorError> {
    let report_data = tdx_attest_rs::tdx_report_data_t {
        d: input_bytes.try_into()?,
    };
    Ok(report_data)
}

/// Creates a TDX report from the given report data.
///
/// # Arguments
///
/// * `report_data` - The report data to use for generating the TDX report
///
/// # Returns
///
/// A `Result` containing the `tdx_report_t` structure on success.
///
/// # Errors
///
/// Returns `QuoteGeneratorError::TdxReportFailed` if the report generation fails.
pub fn create_tdx_report(
    report_data: &tdx_attest_rs::tdx_report_data_t,
) -> Result<tdx_attest_rs::tdx_report_t, QuoteGeneratorError> {
    let mut tdx_report = tdx_attest_rs::tdx_report_t {
        d: [0; REPORT_SIZE],
    };
    let result = tdx_attest_rs::tdx_att_get_report(Some(report_data), &mut tdx_report);
    match result {
        tdx_attest_rs::tdx_attest_error_t::TDX_ATTEST_SUCCESS => Ok(tdx_report),
        _ => {
            error!("Failed to get TDX report: {:?}", result);
            Err(QuoteGeneratorError::TdxReportFailed) // _tdx_attest_error_t does not implement std::error::Error
        }
    }
}

/// Creates a TDX attestation quote from the given report data.
///
/// This function generates a cryptographic quote that can be used to verify
/// the integrity and authenticity of the TDX environment.
///
/// # Arguments
///
/// * `report_data` - The report data to include in the quote
///
/// # Returns
///
/// A `Result` containing the generated quote data as `Vec<u8>` on success.
///
/// # Errors
///
/// * `QuoteGeneratorError::TdxQuoteFailed` - if the quote generation API call fails.
/// * `QuoteGeneratorError::TdxQuoteEmpty` - if the API succeeds but returns no quote data.
pub fn create_quote(
    report_data: &tdx_attest_rs::tdx_report_data_t,
) -> Result<Vec<u8>, QuoteGeneratorError> {
    let mut selected_att_key_id = tdx_attest_rs::tdx_uuid_t {
        d: [0; TDX_UUID_SIZE],
    };
    let (result, quote) = tdx_attest_rs::tdx_att_get_quote(
        Some(report_data),
        None,
        Some(&mut selected_att_key_id),
        0,
    );

    match result {
        tdx_attest_rs::tdx_attest_error_t::TDX_ATTEST_SUCCESS => match quote {
            Some(q) => Ok(q),
            None => Err(QuoteGeneratorError::TdxQuoteEmpty),
        },
        _ => {
            error!("Failed to get TDX quote: {:?}", result);
            Err(QuoteGeneratorError::TdxQuoteFailed) // _tdx_attest_error_t does not implement std::error::Error
        }
    }
}
