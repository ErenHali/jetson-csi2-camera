#!/bin/bash

# This script scans the virtual I2C bus to find ALL responsive devices,
# including the local deserializer and the remote serializer.
# It is designed for devices with 16-bit register addresses.

# !!! Use the VIRTUAL bus number (e.g., 9) !!!
I2C_BUS=9
FOUND_COUNT=0

echo "Scanning I2C bus ${I2C_BUS} for ALL responsive devices..."
echo "Attempting to read register 0x0000 from each possible address..."
echo "--------------------------------------------------------"

# Loop through all valid, non-reserved 7-bit I2C addresses (3 to 119)
for ADDR in $(seq 3 119); do
    HEX_ADDR=$(printf "0x%02x" ${ADDR})

    # The i2ctransfer command for 16-bit register addresses:
    #   w2@... 0x00 0x00 : Write the 16-bit register address 0x0000.
    #   r1@...           : Read 1 byte back (the register's value).
    
    OUTPUT=$(i2ctransfer -y ${I2C_BUS} w2@${HEX_ADDR} 0x00 0x00 r1@${HEX_ADDR} 2>/dev/null)
    EXIT_CODE=$?

    # An exit code of 0 means the transaction was successful (we got an ACK).
    if [ ${EXIT_CODE} -eq 0 ]; then
        echo ">>> Found Device at 7-bit Address: ${HEX_ADDR} <<<"
        echo "    Value read from register 0x0000: ${OUTPUT}"
        echo ""
        FOUND_COUNT=$((FOUND_COUNT + 1))
    fi
done

echo "--------------------------------------------------------"
echo "Scan complete. Found a total of ${FOUND_COUNT} device(s) on bus ${I2C_BUS}."

if [ ${FOUND_COUNT} -eq 0 ]; then
    echo "WARNING: No responsive devices found."
fi
