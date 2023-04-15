fn get_nth_arg(n: usize) -> i32 {
    match std::env::args().nth(n).unwrap().parse::<i32>() {
        Ok(n) => n,
        Err(_) => panic!("Incorrect argument type"),
    }
}

#[derive(Debug)]
pub struct Args {
    pub num_of_zeroes: i32,
    pub n: i32,
    pub max_nonce_len: i32,
}

impl Args {
    pub fn new() -> Self {
        let args: Vec<String> = std::env::args().collect();

        if args.len() < 3 {
            panic!("Incorrent number of arguments: [num_of_zeroes] [n] [max_nonce_len]")
        }

        Args {
            num_of_zeroes: get_nth_arg(1),
            n: get_nth_arg(2),
            max_nonce_len: get_nth_arg(3),
        }
    }
}
