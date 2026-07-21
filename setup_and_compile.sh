#!/bin/bash
set -e

echo "=========================================="
echo "SETUP EMSCRIPTEN + COMPILE WASM MODULES"
echo "=========================================="

# Check if running on Linux
if [[ "$OSTYPE" != "linux-gnu"* ]]; then
    echo "⚠️  Warning: This script is optimized for Linux"
fi

# Step 1: Check if Emscripten is already installed
echo ""
echo "[1/5] Checking Emscripten installation..."
if command -v emcc &> /dev/null; then
    echo "✅ Emscripten already installed"
    emcc --version
else
    echo "⏳ Installing Emscripten..."
    
    # Create emsdk directory if not exists
    if [ ! -d "$HOME/emsdk" ]; then
        git clone https://github.com/emscripten-core/emsdk.git $HOME/emsdk
    fi
    
    cd $HOME/emsdk
    ./emsdk install latest
    ./emsdk activate latest
    source ./emsdk_env.sh
    cd -
    
    echo "✅ Emscripten installed successfully"
fi

# Step 2: Load Emscripten environment
echo ""
echo "[2/5] Loading Emscripten environment..."
if [ -d "$HOME/emsdk" ]; then
    source $HOME/emsdk/emsdk_env.sh
fi
emcc --version
echo "✅ Environment ready"

# Step 3: Verify C++ compiler
echo ""
echo "[3/5] Checking C++ compiler..."
if ! command -v g++ &> /dev/null; then
    echo "⚠️  g++ not found. Installing build-essential..."
    sudo apt-get update
    sudo apt-get install -y build-essential
fi
g++ --version | head -n1
echo "✅ C++ compiler ready"

# Step 4: Create dist directory
echo ""
echo "[4/5] Preparing build directory..."
mkdir -p dist
echo "✅ dist/ directory ready"

# Step 5: Compile all WASM modules
echo ""
echo "[5/5] Compiling all WASM modules..."
echo "⏳ This might take 5-30 minutes depending on your system..."
echo ""

bash build_all.sh

echo ""
echo "=========================================="
echo "✅ COMPILATION SUCCESSFUL!"
echo "=========================================="
echo ""
echo "Generated files in dist/:"
ls -lh dist/ | grep -E "\.(js|wasm)$" || echo "No files found"
echo ""
echo "Total files:"
ls -1 dist/ | wc -l
echo ""
echo "Ready for mobile browser deployment! 🚀"
