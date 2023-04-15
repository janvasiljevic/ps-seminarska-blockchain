extern crate hex;
extern crate hex_slice;
extern crate ocl;
#[macro_use]
extern crate colorify;

use clap::Parser;
use hex_slice::AsHex;
use ocl::{MemFlags, ProQue};

use num_format::{Locale, ToFormattedString};
use std::{
    fs::File,
    io::{self, BufRead, BufReader},
    path::Path,
    time::Instant,
    vec,
};

use rust_gpu::{CustomBufferCreator, CustomBufferReader};

#[derive(Parser, Debug)]
#[clap(author, version, about, long_about = None)]
struct Args {
    /// The file to read input hashes from. Can include empty lines and comments starting with #
    #[clap(short, long)]
    file: String,

    /// The number of zeroes the program will look for
    #[clap(short, long)]
    zeroes: u32,

    /// Maximum nonce length we are lookng for. Must be in range [2, 8]
    #[clap(short, long, default_value_t = 8)]
    max_nonce_len: u32,

    /// Local work size of kernel. Multiple of 32
    #[clap(short, long, default_value_t = 256)]
    local_work_size: u32,

    /// Global work size multiplier. Multiplied by local work size of kernel
    #[clap(short, long, default_value_t = 12)]
    global_work_size_multiplier: u32,

    /// Show how many hashes and at which rate the GPU performs them. Builds a different kernel, which is slightly slower than one without hashrate calculation!
    #[clap(short, long)]
    show_hashrate: bool,
}

fn lines_from_file(filename: impl AsRef<Path>) -> io::Result<Vec<String>> {
    BufReader::new(File::open(filename)?).lines().collect()
}

fn display_hashrate(hash_count: u64, timer: Instant) {
    let hashes = hash_count.to_formatted_string(&Locale::sl);
    let hashes_per_sec = ((hash_count as f64 / timer.elapsed().as_secs_f64()) as u64)
        .to_formatted_string(&Locale::sl);

    println!(
        "> Calculated a total of {} hashes ({} hashes/sec)",
        hashes, hashes_per_sec
    );
}

// For help use: cargo run -- -h
// Example run: cargo run -- -f src/input.sh -z 7 -s
// On nsc: srun -n1 -G1 --reservation=fri ./target/debug/rust-gpu -f input.sh -z 7

fn main() -> ocl::Result<()> {
    let kernel_src_bytes = include_bytes!("./cl-src/kernel.cl");

    let args = Args::parse();
    let input_lines = lines_from_file(args.file).expect("Could not load input file");

    if args.max_nonce_len < 2 || args.max_nonce_len > 8 {
        panic!("Maximum nonce length must be in range [2, 8]!")
    }

    let kernel_src = String::from_utf8_lossy(kernel_src_bytes).replacen(
        "_hashrate_bool_def",
        if args.show_hashrate { "(1)" } else { "(0)" },
        1,
    );

    let que = ProQue::builder()
        .src(kernel_src)
        .build()
        .expect("Queue build failed");

    // Timing function
    let total = Instant::now();
    let mut total_kernel_execution: f64 = 0.0;

    let mut total_of_total_hashes: u64 = 0;

    // How many lines in the input file will we skip (comment or blank)
    let mut skip_count = 0;
    let mut proccessed = 0;

    for input_word in input_lines {
        if input_word.starts_with('#') || input_word.len() < 1 {
            skip_count += 1;
            continue;
        }

        let word: Vec<u8> = hex::decode(&input_word).expect("Decoding failed");
        let mut total_hash_count: u64 = 0;

        printc!(yellow: "Processing word 0x{}... \n", input_word);

        proccessed += 1;

        let timer = Instant::now();

        for nonce_len in 1..args.max_nonce_len {
            let problem_space: u64 = u64::pow(256, nonce_len);
            let mut lock: Vec<i32> = vec![0i32];
            let mut solution: Vec<u32> = vec![0u32; 4];
            let mut nonce_solution: Vec<u64> = vec![0u64];
            let mut total_hashes: Vec<u64> = vec![0u64];

            let word_buffer = &que.new_buffer(&word, Some(MemFlags::new().read_only()));
            let lock_buffer = &que.new_buffer(&lock, None);
            let solution_buffer = &que.new_buffer(&solution, None);
            let nonce_solution_buffer = &que.new_buffer(&nonce_solution, None);
            let total_hashes_buffer = &que.new_buffer(&total_hashes, None);

            let mut kernel_builder = que.kernel_builder("hash");

            kernel_builder
                .local_work_size(args.local_work_size)
                .global_work_size(args.global_work_size_multiplier * args.local_work_size)
                .arg(problem_space)
                .arg(args.zeroes)
                .arg(word.len() as u32)
                .arg(word_buffer)
                .arg(nonce_len as u16)
                .arg(lock_buffer)
                .arg(solution_buffer)
                .arg(nonce_solution_buffer);

            if args.show_hashrate {
                kernel_builder
                    .arg(total_hashes_buffer)
                    .arg_local::<u64>(args.local_work_size as usize);
            }

            let kernel = kernel_builder.build().expect("Building the kernel failed");

            let kernel_execution = Instant::now();

            unsafe {
                kernel.enq().expect("Kernel enquing failed");
            }

            que.finish()?;

            total_kernel_execution += kernel_execution.elapsed().as_secs_f64();

            lock_buffer.read_into(&mut lock);
            solution_buffer.read_into(&mut solution);
            nonce_solution_buffer.read_into(&mut nonce_solution);

            if args.show_hashrate {
                total_hashes_buffer.read_into(&mut total_hashes);
                total_hash_count += total_hashes[0];
            }

            let solution_found = lock[0] != 0;

            if solution_found {
                let mut solution_hex: Vec<u8> = vec![0u8; 4];

                let mut og: u64 = nonce_solution[0];

                for j in 0..nonce_len {
                    solution_hex[j as usize] = (og % 256) as u8;

                    og /= 256;
                }

                println!(
                    "> Solution {:02x} (nonce length {}) found in {:.2?}",
                    solution_hex.plain_hex(true),
                    nonce_len,
                    timer.elapsed(),
                );

                if args.show_hashrate {
                    display_hashrate(total_hash_count, timer);
                    total_of_total_hashes += total_hash_count;
                }

                print!("> Resulting hash: 0x");

                solution.iter().for_each(|y| {
                    let mut og: u32 = *y;
                    let mut letter: u32;

                    for _ in 0..4 {
                        letter = og % 256;
                        print!("{:>02x}", letter);

                        og /= 256;
                    }

                    print!(" ");
                });

                println!();
                println!();

                break;
            } else {
                println!(
                    "...increasing nonce length from {} to {}...",
                    nonce_len,
                    nonce_len + 1
                );
            }
        }
    }

    println!("> Total elapsed time {:.2?}", total.elapsed());
    println!(
        "> Total kernel execution time {:.2}s",
        total_kernel_execution
    );

    if args.show_hashrate {
        display_hashrate(total_of_total_hashes, total);
    }

    println!(
        "> Proccesed {} hashes, Skipped {} lines of the input file",
        proccessed, skip_count
    );

    Ok(())
}
