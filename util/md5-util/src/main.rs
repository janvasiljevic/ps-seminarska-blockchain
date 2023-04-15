extern crate hex;
use md5;

#[derive(Debug)]
enum Errors {
    NotEvenInput,
}

// test: 01020304C2AAD20D
// out: B18F3F414358E488AEFABE8010000000
// num of zeroes: 7
fn main() -> Result<(), Errors> {
    let mut line = String::new();
    println!("> Enter input in HEX: ");
    std::io::stdin().read_line(&mut line).unwrap();

    let strip: &str = &line[..line.len() - 1];

    if strip.len() % 2 != 0 {
        return Err(Errors::NotEvenInput);
    }

    let decoded = hex::decode(strip).expect("Decoding failed");

    let digest = md5::compute(decoded);
    let hex_string = format!("{:x}", digest);
    println!("> MD5: {}", hex_string);
    println!("> Number of zeroes {}", num_of_zeroes(hex_string));

    Ok(())
}

fn num_of_zeroes(hash: String) -> usize {
    for (i, c) in hash.chars().rev().enumerate() {
        if c != '0' {
            return i;
        }
    }

    hash.len()
}
