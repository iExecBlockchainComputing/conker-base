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
use std::process;
use tdx_attest_rs;

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
/// A `tdx_report_data_t` structure containing the input bytes.
///
/// # Panics
///
/// Panics if `input_bytes` length doesn't match `REPORT_DATA_SIZE` (due to `try_into().unwrap()`).
fn create_report_data(input_bytes: &[u8]) -> tdx_attest_rs::tdx_report_data_t {
    let report_data = tdx_attest_rs::tdx_report_data_t {
        d: input_bytes.try_into().unwrap(),
    };
    debug!("TDX report data: {:?}", report_data.d);

    report_data
}

/// Generates and displays a TDX report for the given report data.
///
/// This function creates a TDX report and logs it at debug level.
/// The report is only visible when the logger is configured to show debug messages.
///
/// # Arguments
///
/// * `report_data` - The report data to use for generating the TDX report
///
/// # Exits
///
/// Exits with status code 1 if the report generation fails
fn display_tdx_report(report_data: &tdx_attest_rs::tdx_report_data_t) {
    let mut tdx_report = tdx_attest_rs::tdx_report_t {
        d: [0; REPORT_SIZE],
    };
    let result = tdx_attest_rs::tdx_att_get_report(Some(report_data), &mut tdx_report);
    if result != tdx_attest_rs::tdx_attest_error_t::TDX_ATTEST_SUCCESS {
        error!("Failed to get the report");
        process::exit(1);
    }
    debug!("TDX report: {:?}", tdx_report.d);
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
/// A `Vec<u8>` containing the generated quote data
///
/// # Exits
///
/// Exits with status code 1 if quote generation fails
fn create_quote(report_data: &tdx_attest_rs::tdx_report_data_t) -> Vec<u8> {
    let mut selected_att_key_id = tdx_attest_rs::tdx_uuid_t {
        d: [0; TDX_UUID_SIZE],
    };
    let (result, quote) = tdx_attest_rs::tdx_att_get_quote(
        Some(report_data),
        None,
        Some(&mut selected_att_key_id),
        0,
    );
    if result != tdx_attest_rs::tdx_attest_error_t::TDX_ATTEST_SUCCESS {
        error!("Failed to get the quote");
        process::exit(1);
    }
    match quote {
        Some(q) => {
            debug!("Successfully generated TDX quote with {} bytes", q.len());
            debug!("Quote: {:?}", q);
            q
        }
        None => {
            error!("Failed to get the quote");
            process::exit(1);
        }
    }
}

fn main() {
    // Initialize the logger (defaults to INFO level, override with RUST_LOG env var)
    env_logger::init();

    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        error!("Usage: {} <{}-byte-report-data>", args[0], REPORT_DATA_SIZE);
        process::exit(1);
    }

    let input = &args[1];
    let input_bytes = input.as_bytes();
    if input_bytes.len() > REPORT_DATA_SIZE {
        error!(
            "report_data must be at most {} bytes, got {} bytes",
            REPORT_DATA_SIZE,
            input_bytes.len()
        );
        process::exit(1);
    }

    let mut report_bytes = [0u8; REPORT_DATA_SIZE];
    report_bytes[..input_bytes.len()].copy_from_slice(input_bytes);

    let report_data = create_report_data(&report_bytes);
    display_tdx_report(&report_data); // Report is only displayed on debug mode - this function is optional
    let quote = create_quote(&report_data);
    fs::write(QUOTE_FILE_NAME, quote).expect("Unable to write quote file");
    info!("Quote successfully written to {}", QUOTE_FILE_NAME);
}
