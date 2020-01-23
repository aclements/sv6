use addr2line::Frame;
use clap::{App, Arg};
use curl::easy::Easy;
use serde_derive::Deserialize;
use std::env;

#[derive(Clone, Debug, Deserialize)]
struct ExitTrigger {
    backtrace: Vec<String>,
    count: u64,
}
#[derive(Debug, Deserialize)]
struct QStats {
    exit_triggers: Vec<ExitTrigger>,
}

fn main() {
    let kernel_path = format!("{}/../../o.qemu/kernel.elf", env!("CARGO_MANIFEST_DIR"));

    let matches = App::new("QStats Viewer")
        .version("0.1")
        .arg(
            Arg::with_name("hostname")
                .short("h")
                .long("hostname")
                .default_value("localhost")
                .takes_value(true),
        )
        .arg(
            Arg::with_name("port")
                .short("p")
                .long("port")
                .default_value("8080")
                .takes_value(true),
        )
        .arg(
            Arg::with_name("kernel")
                .short("k")
                .long("kernel")
                .default_value(&kernel_path)
                .takes_value(true),
        )
        .get_matches();

    // Write the contents of rust-lang.org to stdout
    let hostname = matches.value_of("hostname").unwrap();
    let port = matches.value_of("port").unwrap();
    let kernel = matches.value_of("kernel").unwrap();

    let mut dst = Vec::new();
    let mut easy = Easy::new();
    easy.url(&format!("{}:{}/dev/qstats", hostname, port))
        .unwrap();
    {
        let mut transfer = easy.transfer();
        transfer
            .write_function(|data| {
                dst.extend_from_slice(data);
                Ok(data.len())
            })
            .unwrap();
        transfer.perform().unwrap();
    }

    let qstats: QStats = toml::from_slice(&dst).unwrap();

    let kernel_file = std::fs::read(kernel).unwrap();
    let object = addr2line::object::File::parse(&kernel_file).unwrap();
    let context = addr2line::Context::new(&object).unwrap();

    let mut exits = qstats.exit_triggers.clone();
    exits.sort_by_key(|e| e.count);
    exits.reverse();

	let mut skipped = 0;
    let mut total = 0;
    for ExitTrigger { backtrace, count } in exits {
        total += count;
		if count < 10 {
			skipped += 1;
			continue;
		}

        let mut combined_frames = Vec::new();
        for addr in &backtrace {
            let addr = u64::from_str_radix(&addr[2..18], 16).unwrap();
            match context.find_frames(addr) {
                Ok(mut frames) => {
                    while let Ok(Some(Frame {
                        location: Some(location),
                        ..
                    })) = frames.next()
                    {
                        combined_frames.push(location);
                    }
                }
                Err(_) => break,
            }
        }

        for (i, location) in combined_frames.into_iter().enumerate() {
            let file = location.file.unwrap_or("???".to_owned());
            let line = location.line.unwrap_or(0);

            if i == 0 {
                println!("{:>4}: {}:{}", count, file, line);
            } else {
                println!("      {}:{}", file, line);
            }
        }

        let addr = u64::from_str_radix(&backtrace[0][2..18], 16).unwrap();
        println!("      {:x}", addr);
    }

    println!();
    println!("total = {} ({:.1}% skipped)", total, 100.0 * skipped as f32 / total as f32);
}
