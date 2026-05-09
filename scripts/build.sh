#!/bin/bash
set -e  # Exit on any error

echo "Starting build process..."

# Install dependencies
bash ./scripts/install-dependencies.sh

# Compile project
bash ./scripts/compile.sh

echo "Build completed successfully!"
