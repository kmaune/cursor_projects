#!/bin/bash

# Exit on error
set -e

# Install Homebrew if not installed
if ! command -v brew &> /dev/null; then
    echo "Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

# Install CMake if not installed
if ! command -v cmake &> /dev/null; then
    echo "Installing CMake..."
    brew install cmake
fi

# Install Google Test
echo "Installing Google Test..."
brew install googletest

# Install Google Benchmark
echo "Installing Google Benchmark..."
brew install google-benchmark

echo "All dependencies installed successfully!" 