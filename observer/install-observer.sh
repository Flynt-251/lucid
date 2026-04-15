#/bin/bash
echo "This install script requires sudo privileges to install required packages and to add to /usr/bin/"
echo "The following dependencies will be installed with the observer program:"
echo "json-c, json-c-devel, libcurl, libcurl-devel, openssl, openssl-devel, libssh, libssh-devel, freerdp"

if [ "$1" = "--packages-only" ];
	then
	echo "Only dependencies will be installed. Run 'make' in this directory to compile the program."
else
	echo "Clang is required in order to compile the software."
fi
echo ""

confirm="null"
while [ "$confirm" != "y" -a "$confirm" != "n" -a "$confirm" != "Y" -a "$confirm" != "N" -a "$confirm" != "" ]; do
	read -p "Is this okay? [y/N] " confirm
done

if [ "$confirm" = "n" -o "$confirm" = "N" -o "$confirm" = "" ];
	then
	echo "Installation Cancelled."
	exit
fi

sudo echo "Sudo access confirmed."

echo "Installing dependencies via DNF..."
sudo dnf install --assumeyes json-c json-c-devel libcurl libcurl-devel openssl openssl-devel libssh libssh-devel freerdp

if [ "$1" != "--packages-only" ];
	then
	echo "Installing lucid-observer to /usr/bin/..."
	source="https://raw.githubusercontent.com/Flynt-251/lucid/refs/heads/master/observer/observer.c"
	curl "$source" > observer.c
	clang observer.c -o lucid-observer -L/usr/lib64 -L/usr/local/lib -ljson-c -lcurl -lssh
	rm observer.c
	sudo mv ./lucid-observer /usr/bin/

	echo ""
	echo "Installation Complete! The Lucid Observer is available via 'lucid-observer'"
else
	echo ""
	echo "Packages installed."
fi