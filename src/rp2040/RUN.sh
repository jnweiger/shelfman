sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential

(cd pico-sdk; git submodule update --init)
export PICO_SDK_PATH=$PWD/pico-sdk

cd blink
mkdir build
cd build
cmake -DPICO_BOARD=pico ..
make

ls -l $PWD/blink.uf2

echo "Connect USB cable while holding the BOOTSEL button, -> /media/$USER/RPI-RP2/ appears."
echo "cp $PWD/blink.uf2 /media/$USER/RPI-RP2/"

