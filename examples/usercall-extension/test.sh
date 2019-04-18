set -e

# Build custom runner
#cd runner
#cargo +nightly build
#cd -

APPNAME=rust-sgx-ut
APPPATH=../unit_tests
#APPNAME=mpsc-crypto-mining
#APPPATH=../mpsc-crypto-mining
RUNNERPATH=../../target/debug/ftxsgx-runner
# Build APP
cd $APPPATH
cargo +nightly build --target=x86_64-fortanix-unknown-sgx
cd -

# Convert the APP
ftxsgx-elf2sgxs $APPPATH/target/x86_64-fortanix-unknown-sgx/debug/$APPNAME --heap-size 0x20000 --stack-size 0x20000 --threads 6 --debug

# Execute
$RUNNERPATH $APPPATH/target/x86_64-fortanix-unknown-sgx/debug/$APPNAME.sgxs
