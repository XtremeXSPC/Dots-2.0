#!/usr/bin/env bash

set -e # Exit immediately if a command fails

echo "🟢 Starting bootstrap for workstation configuration..."

# Function to execute commands with sudo, asking for password only once
sudo -v
while true; do
	sudo -n true
	sleep 60
	kill -0 "$$" || exit
done 2>/dev/null &

# Operating system detection
if [[ "$(uname)" == "Darwin" ]]; then
	echo "🍎 macOS detected. Setting up environment..."

	# Install Homebrew if not present
	if ! command -v brew &>/dev/null; then
		echo "🍺 Homebrew not found. Installing..."
		/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
		# Add brew to PATH for current session
		eval "$(/opt/homebrew/bin/brew shellenv)"
	else
		echo "🍺 Homebrew already installed."
	fi

	# Install Ansible
	if ! command -v ansible &>/dev/null; then
		echo "📦 Installing Ansible via Homebrew..."
		brew install ansible
	else
		echo "📦 Ansible already installed."
	fi

elif [[ -f "/etc/arch-release" ]]; then
	echo "🐧 Arch Linux detected. Setting up environment..."
	sudo pacman -Syu --noconfirm git ansible
elif [[ -f "/etc/debian_version" ]]; then
	echo "🐧 Debian/Ubuntu detected. Setting up environment..."
	sudo apt-get update
	sudo apt-get install -y git ansible
else
	echo "❌ Unsupported operating system. Exiting."
	exit 1
fi

echo "✅ Prerequisites installed."
echo "⚙️ Running Ansible playbook. Sudo password may be required..."

# Run the main Ansible playbook
ansible-playbook ansible/main.yml --ask-become-pass

echo "Configuration completed! Restart your shell to apply all changes."
