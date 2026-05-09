#!/bin/bash
set -e

echo "Installing dependencies..."

sudo apt update
sudo apt install -y git dpkg-dev gettext-base

git clone https://github.com/WiringPi/WiringPi.git

cd WiringPi
./build debian

cd debian-template/
FILE=$(ls | grep arm64)
sudo apt install -y ./$FILE

cd ../../

sudo apt install -y \
  lxpanel-dev \
  libgtk-3-dev \
  libi2c-dev \
  libc6-dev \
  libglib2.0-dev \
  wiringpi

echo "Dependencies installed."
