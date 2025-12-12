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

use log::{debug, info};
use std::env;
use std::fs;
mod error;
use error::QuoteGeneratorError;
mod utils;
use utils::{create_quote, create_report_data, create_tdx_report};
mod constants;
use constants::REPORT_DATA_SIZE;

fn main() -> Result<(), QuoteGeneratorError> {
    // Initialize the logger (defaults to INFO level, override with RUST_LOG env var)
    env_logger::init();

    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        return Err(QuoteGeneratorError::InvalidUsage {
            actual: args.len() - 1,
        });
    }

    let input = &args[1];
    let input_bytes = input.as_bytes();
    if input_bytes.len() > REPORT_DATA_SIZE {
        return Err(QuoteGeneratorError::ReportDataTooLarge {
            max: REPORT_DATA_SIZE,
            actual: input_bytes.len(),
        });
    }

    let mut report_bytes = [0u8; REPORT_DATA_SIZE];
    report_bytes[..input_bytes.len()].copy_from_slice(input_bytes);

    let report_data = create_report_data(&report_bytes)?;
    debug!("Report data: {:?}", report_data.d);
    if log::log_enabled!(log::Level::Debug) {
        let tdx_report = create_tdx_report(&report_data)?;
        println!("TDX report: {:?}", tdx_report.d);
    }
    let quote = create_quote(&report_data)?;
    debug!("Quote: {:?}", quote);
    let quote_filename = format!("quote-{}.dat", input);
    fs::write(&quote_filename, quote)?;
    info!("Quote successfully written to {}", quote_filename);

    Ok(())
}
