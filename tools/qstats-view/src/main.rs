use addr2line::Frame;
use clap::{App, Arg};
use curl::easy::Easy;
use serde_derive::Deserialize;
use std::collections::BTreeMap;
use std::env;

#[derive(Debug, Deserialize)]
struct QStats {
    rips: BTreeMap<String, u64>,
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

    let mut rips: Vec<(String, u64)> = qstats
        .rips
        .iter()
        .map(|(addr, count)| (addr.clone(), *count))
        .collect();
    rips.sort_by_key(|r| r.1);
    rips.reverse();

    for (addr, count) in rips.iter() {
        let addr = u64::from_str_radix(&addr[2..18], 16).unwrap();

        if let Ok(mut frames) = context.find_frames(addr) {
            if let Ok(Some(Frame {
                location: Some(location),
                ..
            })) = frames.next()
            {
                println!(
                    "{:>4}: {}:{}",
                    count,
                    location.file.unwrap_or("???".to_owned()),
                    location.line.unwrap_or(0),
                )
            }
            while let Ok(Some(Frame {
                location: Some(location),
                ..
            })) = frames.next()
            {
                println!(
                    "      {}:{}",
                    location.file.unwrap_or("???".to_owned()),
                    location.line.unwrap_or(0),
                )
            }
             println!("      {:x}", addr);
        }
    }

    println!();
    println!("total = {}", rips.iter().map(|r| r.1).sum::<u64>());
}
