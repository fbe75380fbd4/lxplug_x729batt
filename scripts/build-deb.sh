#!/bin/bash
set -e

echo "Building .deb package"

# Check if version is provided
if [ -z "$DEB_VERSION" ]; then
  echo "Error: DEB_VERSION not set"
  exit 1
fi

# Install binaries
sudo apt install dpkg-dev debhelper

PROJECT_NAME="lxplug-x729batt"
PACKAGE_DIR="${PROJECT_NAME}-${DEB_VERSION}"
DEB_DIR="$PACKAGE_DIR/debian"

# Create package structure
mkdir -p "$DEB_DIR"
mkdir -p "$PACKAGE_DIR/usr/lib/aarch64-linux-gnu/lxpanel/plugins"

# Copy files from template folder
cp debian-template/changelog $DEB_DIR/changelog
cp debian-template/control $DEB_DIR/control
cp debian-template/copyright $DEB_DIR/copyright
cp debian-template/install $DEB_DIR/install
cp debian-template/postinst $DEB_DIR/postinst
cp debian-template/postrm $DEB_DIR/postrm
cp debian-template/rules $DEB_DIR/rules

# Copy compiled file
cp x729batt.so $PACKAGE_DIR/usr/lib/aarch64-linux-gnu/lxpanel/plugins/x729batt.so

# Build the .deb package
cd $PACKAGE_DIR
dpkg-buildpackage -us -uc -b

echo ".deb package created successfully"
