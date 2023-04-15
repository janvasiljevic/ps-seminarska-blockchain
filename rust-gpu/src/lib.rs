extern crate ocl;

use ocl::{Buffer, MemFlags, ProQue};

pub trait CustomBufferCreator {
    fn new_buffer<T>(&self, vector: &Vec<T>, mem_flag: Option<MemFlags>) -> ocl::Buffer<T>
    where
        T: ocl::OclPrm;
}

pub trait CustomBufferReader<T> {
    fn read_into(&self, vector: &mut Vec<T>);
}

impl CustomBufferCreator for ProQue {
    fn new_buffer<T>(&self, vector: &Vec<T>, mem_flag: Option<MemFlags>) -> ocl::Buffer<T>
    where
        T: ocl::OclPrm,
    {
        Buffer::builder()
            .queue(self.queue().clone())
            .flags(mem_flag.unwrap_or(MemFlags::new().read_write()))
            .len(vector.len())
            .copy_host_slice(vector)
            .build()
            .expect("Error creating buffer")
    }
}

impl<T> CustomBufferReader<T> for Buffer<T>
where
    T: ocl::OclPrm,
{
    fn read_into(&self, vector: &mut Vec<T>) {
        self.read(vector).enq().expect("Error reading buffer")
    }
}
