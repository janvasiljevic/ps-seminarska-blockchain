# Quick start

For **help** use
```s
cargo run -- -h
```

Example command for running in debug mode
```s
cargo run -- -f src/input.sh -z 7 -s
```

Build an optimized *release* version (used for benchmarks)
```s
cargo b --release
```

Running on **Windows**
```s
.\target\release\rust-gpu.exe -f src/input.sh -z 7 -s -l 64
```

Running on **NSC**
```s
module load Rust
module load CUDA

cargo b --release

srun -n1 -G1 --reservation=fri ./target/release/rust-gpu -f src/input.sh -z 7
```