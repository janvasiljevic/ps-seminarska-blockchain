extern crate hex;
extern crate rustc_serialize;
use md5;
use rand::{distributions::Alphanumeric, Rng};
use rustc_serialize::hex::ToHex;

mod args;
use args::Args;

#[derive(Debug)]
enum Errors {}

fn main() -> Result<(), Errors> {
    let args = Args::new();

    let mut n_generated = 0;

    let mut hex_list: Vec<String> = Vec::with_capacity(args.n as usize);
    let mut hash_list: Vec<String> = Vec::with_capacity(args.n as usize);

    while n_generated < args.n {
        let rnd_str: String = rand::thread_rng()
            .sample_iter(&Alphanumeric)
            .take(
                rand::thread_rng()
                    .gen_range(args.max_nonce_len as usize..4 + args.max_nonce_len as usize),
            )
            .map(char::from)
            .collect();

        let digest = md5::compute(&rnd_str);

        let hash_hex = format!("{:x}", digest);

        let num_of_zeroes = get_num_of_zeroes(&hash_hex) as i32;

        if num_of_zeroes == args.num_of_zeroes {
            hex_list.push(rnd_str.as_bytes().to_hex());
            hash_list.push(hash_hex);

            n_generated += 1;
        }
    }

    println!("------- WORDS AND NONCES -------");
    for i in 0..args.n {
        let v: usize = i as usize;
        let splice: usize = rand::thread_rng().gen_range(1..args.max_nonce_len) as usize * 2;

        println!(
            "{} {}",
            &hex_list[v][0..hex_list[v].len() - splice],
            &hex_list[v][hex_list[v].len() - splice..]
        );
    }
    println!("------------ HASHES ------------");
    for i in 0..args.n {
        println!("{}", hash_list[i as usize]);
    }

    Ok(())
}

fn get_num_of_zeroes(hash: &String) -> usize {
    for (i, c) in hash.chars().rev().enumerate() {
        if c != '0' {
            return i;
        }
    }

    hash.len()
}
