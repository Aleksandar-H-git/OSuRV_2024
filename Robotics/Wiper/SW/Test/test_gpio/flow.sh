#!/bin/bash

exit 0


./waf configure

# Test robot.
./waf build && ./build/test_gpio 10 w 1
./waf build && ./build/test_gpio 10 w 0
./waf build && ./build/test_gpio 10 r
