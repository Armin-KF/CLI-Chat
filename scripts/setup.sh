#!/bin/bash

# CLI-Chat Setup Script
# This script sets up the development environment and builds the project

set -e

echo "=== CLI-Chat Setup Script ==="
echo "Setting up development environment..."

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running on supported platform
check_platform() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        PLATFORM="linux"
        PACKAGE_MANAGER="apt"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        PLATFORM="macos"
        PACKAGE_MANAGER="brew"
    else
        print_error "Unsupported platform: $OSTYPE"
        exit 1
    fi
    print_status "Detected platform: $PLATFORM"
}

# Install system dependencies
install_dependencies() {
    print_status "Installing system dependencies..."

    if [[ "$PLATFORM" == "linux" ]]; then
        sudo apt update
        sudo apt install -y \
            build-essential \
            cmake \
            libboost-all-dev \
            libssl-dev \
            libsqlite3-dev \
            libhiredis-dev \
            pkg-config \
            nodejs \
            npm \
            docker.io \
            docker-compose
    elif [[ "$PLATFORM" == "macos" ]]; then
        if ! command -v brew &> /dev/null; then
            print_error "Homebrew not found. Please install it from https://brew.sh/"
            exit 1
        fi

        brew install \
            boost \
            openssl \
            sqlite3 \
            hiredis \
            cmake \
            node \
            npm \
            docker \
            docker-compose
    fi
}

# Create project directories
create_directories() {
    print_status "Creating project directories..."

    mkdir -p data
    mkdir -p certs
    mkdir -p logs

    print_status "Directories created successfully"
}

# Generate SSL certificates for development
generate_certificates() {
    print_status "Generating SSL certificates for development..."

    if [[ ! -f "certs/server.crt" || ! -f "certs/server.key" ]]; then
        openssl req -x509 -newkey rsa:4096 -keyout certs/server.key -out certs/server.crt \
            -days 365 -nodes -subj "/C=US/ST=Dev/L=Local/O=CLI-Chat/CN=localhost"
        print_status "SSL certificates generated"
    else
        print_warning "SSL certificates already exist"
    fi
}

# Build C++ backend
build_backend() {
    print_status "Building C++ backend..."

    cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

    cmake --build build --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

    print_status "Backend built successfully"
}

# Setup frontend
setup_frontend() {
    print_status "Setting up React frontend..."

    cd frontend
    npm install

    # Create production build
    npm run build

    cd ..
    print_status "Frontend setup completed"
}

# Run tests
run_tests() {
    print_status "Running tests..."

    # Backend tests
    if [[ -f "build/tests" ]]; then
        ./build/tests
        print_status "Backend tests passed"
    else
        print_warning "Backend tests not found, skipping..."
    fi

    # Frontend tests
    cd frontend
    npm test -- --watchAll=false --coverage
    cd ..

    print_status "All tests completed"
}

# Create systemd service file (Linux only)
create_service_file() {
    if [[ "$PLATFORM" == "linux" ]]; then
        print_status "Creating systemd service file..."

        cat > cli-chat.service << EOF
[Unit]
Description=CLI-Chat Server
After=network.target
Requires=redis.service

[Service]
Type=simple
User=chat
Group=chat
WorkingDirectory=/opt/cli-chat
ExecStart=/opt/cli-chat/build/chat_server config/server.conf
Restart=always
RestartSec=10

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/opt/cli-chat/data /opt/cli-chat/logs

[Install]
WantedBy=multi-user.target
EOF

        print_status "Systemd service file created: cli-chat.service"
        print_warning "To install: sudo cp cli-chat.service /etc/systemd/system/"
    fi
}

# Setup development environment
setup_dev_environment() {
    print_status "Setting up development environment..."

    # Create .env file for development
    cat > .env << EOF
# Development environment variables
CHAT_SERVER_HOST=127.0.0.1
CHAT_SERVER_PORT=4000
CHAT_TLS_ENABLED=true
CHAT_CERT_FILE=certs/server.crt
CHAT_KEY_FILE=certs/server.key
CHAT_DATABASE_PATH=data/chat.db
CHAT_REDIS_HOST=127.0.0.1
CHAT_REDIS_PORT=6379
CHAT_MAX_USERS=100
CHAT_WEB_PORT=8080
EOF

    # Create docker-compose override for development
    cat > docker-compose.override.yml << EOF
version: '3.8'

services:
  chat-server:
    build:
      target: development
    volumes:
      - .:/app
      - /app/build
    environment:
      - CMAKE_BUILD_TYPE=Debug
    command: ["./scripts/dev-server.sh"]

  web-frontend:
    build:
      context: ./frontend
      target: development
    volumes:
      - ./frontend:/app
      - /app/node_modules
    command: ["npm", "start"]
    ports:
      - "3000:3000"
EOF

    print_status "Development environment configured"
}

# Main setup function
main() {
    echo "Starting CLI-Chat setup..."

    check_platform

    # Parse command line arguments
    SKIP_DEPS=false
    RUN_TESTS=false
    DEV_SETUP=false

    while [[ $# -gt 0 ]]; do
        case $1 in
            --skip-deps)
                SKIP_DEPS=true
                shift
                ;;
            --run-tests)
                RUN_TESTS=true
                shift
                ;;
            --dev)
                DEV_SETUP=true
                shift
                ;;
            -h|--help)
                echo "Usage: $0 [options]"
                echo "Options:"
                echo "  --skip-deps    Skip system dependency installation"
                echo "  --run-tests    Run tests after building"
                echo "  --dev          Setup development environment"
                echo "  -h, --help     Show this help message"
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                exit 1
                ;;
        esac
    done

    if [[ "$SKIP_DEPS" == false ]]; then
        install_dependencies
    fi

    create_directories
    generate_certificates
    build_backend
    setup_frontend

    if [[ "$RUN_TESTS" == true ]]; then
        run_tests
    fi

    if [[ "$DEV_SETUP" == true ]]; then
        setup_dev_environment
    fi

    create_service_file

    echo ""
    print_status "Setup completed successfully!"
    echo ""
    echo "Next steps:"
    echo "1. Start Redis server: redis-server"
    echo "2. Run the chat server: ./build/chat_server config/server.conf"
    echo "3. Access web interface at: http://localhost:8080"
    echo ""
    echo "For development:"
    echo "- Use Docker Compose: docker-compose up -d"
    echo "- Check logs: docker-compose logs -f"
    echo ""
    echo "For production deployment, see README.md"
}

# Run main function
main "$@"