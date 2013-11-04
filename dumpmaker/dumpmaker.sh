#!/bin/bash

st-term | ./streamfilter | pv > $1
