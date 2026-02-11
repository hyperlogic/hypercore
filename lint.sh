#!/bin/bash
cpplint --repository=. --exclude=src/optionparser.h --filter=-build/c++11,-build/c++17 --linelength=120 --recursive src