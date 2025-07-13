# XtremeXSPC's Dotfiles

![https://img.shields.io/badge/managed--by-Ansible-blueviolet](https://img.shields.io/badge/managed--by-Ansible-blueviolet) ![https://img.shields.io/badge/os-macOS%20%7C%20Linux-lightgrey](https://img.shields.io/badge/os-macOS%20%7C%20Linux-lightgrey)

This repository contains my personal development environment configuration, managed by **Ansible**. The goal is to create a fully automated, reproducible, and robust setup that can be deployed on a new macOS or Linux (Arch/Debian-based) machine with a single command.

---

## Philosophy

The core principle behind this setup is **Infrastructure as Code (IaC)** applied to a personal workstation. Instead of manually installing packages and copying configuration files, this environment is defined declaratively using Ansible playbooks.

- **Automated**: The entire setup is handled by a single bootstrap script.
- **Reproducible**: Guarantees a consistent environment across multiple machines.
- **Idempotent**: The setup script can be run multiple times without causing issues. Ansible ensures that only necessary changes are made.
- **Self-contained**: This repository contains both the configuration files (dotfiles) and the automation logic to deploy them.

## Quickstart - Deploying the environment

To deploy this configuration on a new machine, follow these three simple steps:

1. **Clone the repository:**

    ```sh
    git clone https://github.com/XtremeXSPC/Dotfiles.git
    ```

2. **Navigate into the directory:**

    ```sh
    cd Dotfiles
    ```

3. **Run the bootstrap script:**

    ```sh
    ./bootstrap.sh
    ```

The script will take care of everything: installing dependencies like Homebrew (on macOS) and Ansible, and then running the main Ansible playbook to configure the system.

---

## How it works

This setup leverages a few key components:

- **`bootstrap.sh`**: The main entrypoint. This script detects the operating system, installs essential dependencies (like `git` and Ansible itself), and then launches the main Ansible playbook.

- **`ansible/`**: This directory contains all the automation logic.
  - **`main.yml`**: The main playbook that orchestrates which configuration steps to run.
  - **`roles/`**: The configuration is broken down into modular roles (e.g., `zsh`, `devtools`, `neovim`). Each role is responsible for installing a specific application and deploying its configuration files. This makes the setup easy to understand and extend.

- **`config/`**: This directory holds all the actual configuration files (the "dots"). The Ansible playbooks are responsible for creating symbolic links from this directory to the correct locations in the user's home directory (e.g., from `config/zsh/.zshrc` to `~/.zshrc`).

### Repository structure

```Tree
.
├── ansible/                # Ansible playbooks and roles
│   ├── main.yml
│   └── roles/
│       ├── common/
│       ├── devtools/
│       └── zsh/
├── config/                 # The actual configuration files
│   ├── nvim/
│   ├── zsh/
│   └── ...
├── .gitignore
├── bootstrap.sh            # The only script you need to run
└── README.md
```

---

## Making changes and adding new configurations

The modular structure makes it simple to modify the environment. To add a new application (e.g., `tmux`):

1. **Add config files**: Place your `tmux` configuration files in a new directory: `config/tmux/`.
2. **Create a new role**: Create a new role within Ansible: `ansible/roles/tmux/tasks/main.yml`.
3. **Define tasks**: In the new `main.yml`, define the tasks to:
    - Install `tmux` using the appropriate package manager (Homebrew for macOS, Pacman/Apt for Linux).
    - Create a symbolic link from `config/tmux/.tmux.conf` to `~/.tmux.conf`.
4. **Update the main playbook**: Add the new `tmux` role to `ansible/main.yml`.
5. **Run the playbook**: Re-run `./bootstrap.sh` or `ansible-playbook ansible/main.yml`. Ansible will apply only the new changes.

---

## Acknowledgements

This setup is inspired by the best practices of the dotfiles and infrastructure-as-code communities. It relies on fantastic open-source tools like:

- [Ansible](https://www.ansible.com/)
- [Homebrew](https://brew.sh/)
- [Oh My Zsh](https://ohmyz.sh/)
- And many other tools configured within.

## Contributing

Contributions are welcome! Feel free to open an issue or submit a pull request if you have suggestions for improving this repository.

## License

This repository is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
