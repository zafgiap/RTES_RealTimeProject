#!/bin/bash

# Already in /RTES_RealTimeProject root directory

read -r -p "Enter the Raspberry Pi target (name@IP): " rpi_target
if [[ -z "$rpi_target" ]]; then
	printf '%s\n' "A Raspberry Pi target is required." >&2
	exit 1
fi

# Clean and compile the project.
make clean || exit 1
make || exit 1

# Copy the executable, start it remotely, and open an SSH session.
scp main "$rpi_target:~" || exit 1
ssh "$rpi_target" 'nohup stdbuf -oL -eL ./main > main.log 2>&1 < /dev/null &'
ssh "$rpi_target"