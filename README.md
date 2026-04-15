# Lucid - Lightweight Computer Lab Management for Modern Linux

Lucid works differently to existing computer lab management systems, solving its main problem: Wayland Support.

> PLEASE NOTE: The below instructions for installation will not work, as the github repository is currently privated in order to maintain academic integrity. This software should be compiled by installing dependencies using the respective options, then compiling the server or observer respectively.
> 
> Example:
> ```sh
> chmod u+x ./install-observer.sh
> ./install-observer.sh --packages-only
> ...
> make
> ```

## Overview

Lucid is split into three parts: the server, observer devices, and client devices. The server connects to a MariaDB database, and over an HTTPS server, serves observers the list of client devices available for remote management. The observers connect to these client devices over an interface which facilitates both SSH and RDP connections.

## Server Installation

`curl https://raw.githubusercontent.com/Flynt-251/lucid/refs/heads/master/server/install-server.sh | sh`

## Observer Installation

`curl https://raw.githubusercontent.com/Flynt-251/lucid/refs/heads/master/observer/install-observer.sh | sh`

## Client Installation

`curl https://raw.githubusercontent.com/Flynt-251/lucid/refs/heads/master/client/install-client.sh | sh`

## Manual Installation

First pull the source entire source code, and navigate to that folder. Then, follow the instructions for each of the software components.

### Server

```sh
cd server
sudo chmod +x ./install-server.sh
./install-server.sh --manual-build
...
make all
```

Follow the prompts to configure the system for use with the server, ensuring `clang` is installed so that the source files can be compiled. Ensure port 8888 is available to serve the client list.

### Observer

```sh
cd observer
sudo chmod +x ./install-observer.sh
./install-server.sh --packages-only
...
make
```

Ensure `clang` is installed, so that the source code can be compiled.

### Client

Proceeds the same way as normal, see [above](#client-installation).