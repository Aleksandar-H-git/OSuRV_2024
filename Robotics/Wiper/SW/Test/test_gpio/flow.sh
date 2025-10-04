#!/bin/bash

exit 0


./waf configure

# Test robot.
./waf build && ./build/test_gpio w 2 1 # Set pin 2 to logical 1
./waf build && ./build/test_gpio w 2 0 # Clear pin 2 to logical 0
./waf build && ./build/test_gpio r 22 # Read from pin 22
./waf build && ./build/test_gpio u 22 # Read from pin 22 with pull-up on
./waf build && ./build/test_gpio u 22 # Read from pin 22 with pull-down on

