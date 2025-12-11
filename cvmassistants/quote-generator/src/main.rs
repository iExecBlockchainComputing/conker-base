/*
 * Copyright (C) 2011-2022 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

use log::{debug, error, info};
use std::env;
use std::fs;
mod error;
use error::QuoteGeneratorError;

const REPORT_DATA_SIZE: usize = 64;
const REPORT_SIZE: usize = 1024;
const TDX_UUID_SIZE: usize = 16;
const QUOTE_FILE_NAME: &str = "quote.dat";

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
fn create_report_data(input_bytes: &[u8]) -> Result<tdx_attest_rs::tdx_report_data_t, QuoteGeneratorError> {
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
fn create_tdx_report(report_data: &tdx_attest_rs::tdx_report_data_t) -> Result<tdx_attest_rs::tdx_report_t, QuoteGeneratorError> {
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
fn create_quote(report_data: &tdx_attest_rs::tdx_report_data_t) -> Result<Vec<u8>, QuoteGeneratorError> {
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
        tdx_attest_rs::tdx_attest_error_t::TDX_ATTEST_SUCCESS => {
            match quote {
                Some(q) => Ok(q),
                None => Err(QuoteGeneratorError::TdxQuoteEmpty),
            }
        },
        _ => {
            error!("Failed to get TDX quote: {:?}", result);
            Err(QuoteGeneratorError::TdxQuoteFailed) // _tdx_attest_error_t does not implement std::error::Error
        }
    }
}

fn main() -> Result<(), QuoteGeneratorError> {
    // Initialize the logger (defaults to INFO level, override with RUST_LOG env var)
    env_logger::init();

    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        return Err(QuoteGeneratorError::InvalidUsage { actual: args.len() });
    }

    let input = &args[1];
    let input_bytes = input.as_bytes();
    if input_bytes.len() > REPORT_DATA_SIZE {
        return Err(QuoteGeneratorError::ReportDataTooLarge { max: REPORT_DATA_SIZE, actual: input_bytes.len() });
    }

    let mut report_bytes = [0u8; REPORT_DATA_SIZE];
    report_bytes[..input_bytes.len()].copy_from_slice(input_bytes);

    let report_data = create_report_data(&report_bytes)?;
    debug!("Report data: {:?}", report_data.d);
    let tdx_report = create_tdx_report(&report_data)?; // Optional function
    debug!("TDX report: {:?}", tdx_report.d);
    let quote = create_quote(&report_data)?;
    debug!("Quote: {:?}", quote);
    fs::write(QUOTE_FILE_NAME, quote)?;
    info!("Quote successfully written to {}", QUOTE_FILE_NAME);

    Ok(())
}
