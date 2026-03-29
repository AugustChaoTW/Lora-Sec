#!/bin/bash

set -e

echo "📦 Building Tamarin Docker image..."
docker build -f Dockerfile.tamarin -t lora-mesh-tamarin:latest .

echo "✅ Tamarin image built successfully!"
echo ""
echo "To run Tamarin:"
echo "  docker run -it --rm -v \$(pwd):/workspace lora-mesh-tamarin:latest tamarin-prover --version"
echo ""
echo "To run Tamarin interactively:"
echo "  docker run -it --rm -v \$(pwd):/workspace lora-mesh-tamarin:latest bash"

