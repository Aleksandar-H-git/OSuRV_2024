#!/bin/bash

exit 0


./waf configure

# Test robot.
./waf build && ./build/test_gpio w 2 1 # Set pin 2 to logical 1
./waf build && ./build/test_gpio w 2 0 # Clear pin 2 to logical 0
./waf build && ./build/test_gpio r 22 # Read from pin 22
./waf build && ./build/test_gpio u # Set pull-up on all input pins
./waf build && ./build/test_gpio d # Set pull-down on all input pins
./waf build && ./build/test_gpio n # No pull-up/down
