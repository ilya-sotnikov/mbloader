# mbloader

Modbus RTU firmware update utility written in C++/Qt.

Bootloader implementation: [stm32-modbus-bootloader](https://github.com/ilya-sotnikov/stm32-modbus-bootloader).

## Usage

`mbloader -n /dev/ttyUSB0 -a 1 stm32f103-app.bin`

```
Usage: ./mbloader [options] firmware
Modbus loader

Options:
  -h, --help      Displays help on commandline options.
  --help-all      Displays help including Qt specific options.
  -n <port>       Port name.
  -b <baud rate>  Baud rate. (115200)
  -d <data bits>  Data bits. (8)
  -p <parity>     Parity. (N)
  -s <stop bits>  Stop bits. (1)
  -a <address>    Server address.

Arguments:
  firmware        A bin file to load.
```

## License

- mbloader -- Apache-2.0
- Qt 6 -- LGPL-3.0

## Build instructions

You can build it in all OSs where Qt6 is available.

Dependencies:
- Qt6 (base, QSerialBus)

Install them using a package manager or using the Qt installer.

To build and install:

1. `git clone https://github.com/ilya-sotnikov/mbloader`
2. `cd mbloader`
3. `mkdir build && cd build`
4. `cmake -DCMAKE_BUILD_TYPE=Release -G Ninja .. && cmake --build .`
5. `cmake --install .`

## Modbus user function

Function code = 0x6c (110)

Format:

- server address -- 1 byte
- PDU -- 253 bytes max
- CRC -- 2 bytes

PDU:

- function code -- 1 byte
- subfunction code -- 1 byte
- subfunction data -- 251 bytes max

Subfunction data:

- depends on the subfunction

Subfunctions:

- 0 -- erase flash
- 1 -- program flash
- 2 -- get checksum
- 3 -- reset device

### Subfunctions format

#### Requests

Erase flash, get checksum, reset device:

- no subfunction data

Program flash:

- flash offset -- 4 bytes
- flash data -- 246 bytes max

#### Responses

Erase flash, program flash, reset device:

- status -- 1 byte

Status == 0 means no error.

Get checksum:

- checksum -- 1 byte

The checksum is calculated as:
```C
uint8_t checksum = 0;

for (uint32_t i = 0; i < fw_size_bytes; ++i)
    checksum += firmware[i];
```

#### Examples

Server address and CRC are not considered, only PDU.

#### Reset device

Request:

`6c 03`

- 6c -- function code
- 03 -- subfunction code

Response:

`6c 03 00`

- 6c -- function code
- 03 -- subfunction code
- 00 -- status (no error)

#### Program flash

Request:

`6c 01 ce 04 00 00 ... [flash data, 246 bytes max]`

- 6c -- function code
- 01 -- subfunction code
- ce 04 00 00 -- offset in little-endian (== 1230), 
- flash data -- 246 bytes max, in little-endian

Response:

`6c 01 00`

- 6c -- function code
- 01 -- subfunction code
- 00 -- status (no error)

## Contributing

Feel free to add features or fix bugs. This project uses the Qt code style, you 
can use the .clang-format file from 
[here](https://code.qt.io/cgit/qt/qt5.git/tree/_clang-format).

Debug build uses sanitizers (address, undefined, leak) by default.
