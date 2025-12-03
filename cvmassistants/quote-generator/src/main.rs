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
use tdx_attest_rs;

const REPORT_DATA_SIZE: usize = 64;
const REPORT_SIZE: usize = 1024;
const TDX_UUID_SIZE: usize = 16;

fn main() {
    // Initialize the logger (defaults to INFO level, override with RUST_LOG env var)
    env_logger::init();

    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        error!("Usage: {} <64-byte-report-data>", args[0]);
        return;
    }

    let input = &args[1];
    let input_bytes = input.as_bytes();
    if input_bytes.len() != REPORT_DATA_SIZE {
        error!(
            "report_data must be exactly 64 bytes, got {} bytes",
            input_bytes.len()
        );
        return;
    }

    let mut report_data_bytes = [0u8; REPORT_DATA_SIZE];
    report_data_bytes.copy_from_slice(input_bytes);

    let report_data = tdx_attest_rs::tdx_report_data_t {
        d: report_data_bytes,
    };
    debug!("TDX report data: {:?}", report_data.d);

    let mut tdx_report = tdx_attest_rs::tdx_report_t { d: [0; REPORT_SIZE] };
    let result = tdx_attest_rs::tdx_att_get_report(Some(&report_data), &mut tdx_report);
    if result != tdx_attest_rs::tdx_attest_error_t::TDX_ATTEST_SUCCESS {
        error!("Failed to get the report");
        return;
    }
    debug!("TDX report: {:?}", tdx_report.d);

    let mut selected_att_key_id = tdx_attest_rs::tdx_uuid_t { d: [0; TDX_UUID_SIZE] };
    let (result, quote) = tdx_attest_rs::tdx_att_get_quote(
        Some(&report_data),
        None,
        Some(&mut selected_att_key_id),
        0,
    );
    if result != tdx_attest_rs::tdx_attest_error_t::TDX_ATTEST_SUCCESS {
        error!("Failed to get the quote");
        return;
    }
    match quote {
        Some(q) => {
            debug!("Successfully generated TDX quote with {} bytes", q.len());
            debug!("Quote: {:?}", q);
            fs::write("quote.dat", q).expect("Unable to write quote file");
            info!("Quote successfully written to quote.dat");
        }
        None => {
            error!("Failed to get the quote");
            return;
        }
    }
}
