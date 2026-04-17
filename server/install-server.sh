#/bin/bash
echo "This install script requires sudo privileges to install required packages and to add to /usr/bin/"
echo "The following dependencies will be installed with the observer program:"
echo "mariadb-server, mariadb-connector-c, mariadb-connector-c-devel argon2, libargon2, json-c, json-c-devel, libmicrohttpd, libmicrohttpd-devel, openssl, openssl-devel"

if [ "$1" = "--packages-only" ];
	then
	echo "Only dependencies will be installed. Run 'make' in this directory to compile the program."
elif [ "$1" = "--manual-build" ];
	then
	echo "Use 'make' to compile the source code. All other components will be prepared as normal."
else
	echo "Clang is required in order to compile the software. For MariaDB, you will need to provide a user that can create a new database and user."
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
sudo dnf install --assumeyes mariadb-server mariadb-connector-c mariadb-connector-c-devel argon2 libargon2 libargon2-devel json-c json-c-devel libmicrohttpd libmicrohttpd-devel openssl openssl-devel

if [ "$1" != "--packages-only" ];
	then

	if [ "$1" != "--manual-build" ];
		then
		echo "Downloading all required files..."

		neon_source="https://raw.githubusercontent.com/Flynt-251/lucid/refs/heads/master/server/neon2.c"
		server_source="https://raw.githubusercontent.com/Flynt-251/lucid/refs/heads/master/server/lucid-server.c"
		client_mgmt_source="https://raw.githubusercontent.com/Flynt-251/lucid/refs/heads/master/server/client-manager.c"
		admin_mgmt_source="https://raw.githubusercontent.com/Flynt-251/lucid/refs/heads/master/server/administrator-panel.c"
		schema_source="https://raw.githubusercontent.com/Flynt-251/lucid/refs/heads/master/server/schema.sql"
		html_one_source="https://raw.githubusercontent.com/Flynt-251/lucid/refs/heads/master/server/management-index.html.part1"
		html_two_source="https://raw.githubusercontent.com/Flynt-251/lucid/refs/heads/master/server/management-index.html.part2"
		
		curl "$neon_source" > neon2.c
		curl "$server_source" > lucid-server.c
		curl "$client_mgmt_source" > client-manager.c
		curl "$admin_mgmt_source" > administrator-panel.c
		curl "$schema_source" > schema.sql
		curl "$html_one_source" > management_index.html.part1
		curl "$html_two_source" > management_index.html.part2

		echo "Installing lucid-server to /usr/bin/..."

		clang neon2.c -largon2 -c -o neon2
		clang lucid-server.c -o lucid-server -I/usr/include/mysql -I/usr/include/mysql/mysql -L/usr/lib64 -L/usr/local/lib -lmariadb -largon2 -ljson-c -lmicrohttpd neon2
		clang client-manager.c -o lucid-client-management -I/usr/include/mysql -I/usr/include/mysql/mysql -L/usr/lib64 -L/usr/local/lib -lmariadb -largon2 neon2
		clang administrator-panel.c -o lucid-admin-panel -I/usr/include/mysql -I/usr/include/mysql/mysql -L/usr/lib64 -L/usr/local/lib -lmariadb -largon2 neon2
		rm neon2.c lucid-server.c client-manager.c administrator-panel.c neon2
		sudo mv ./lucid-server ./lucid-client-management ./lucid-admin-panel /usr/bin/
	fi

	echo "Setting up MariaDB, please follow the prompts to proceed through secure installation."
	echo "This installation script will create a database using the root account. You will need to enter its password after installation."
	sudo systemctl start mariadb
	sudo systemctl enable mariadb
	sudo mariadb-secure-installation
	mariadb -u root -p < schema.sql
	if [ "$1" != "--manual-build" ];
		then
		rm schema.sql
	fi

	echo "Creating lucid user and paths..."
	sudo useradd lucid -r -s /usr/sbin/nologin
	sudo mkdir /etc/lucid-server
	sudo chmod a+r /etc/lucid-server
	sudo chown -R lucid:lucid /etc/lucid-server
	sudo mkdir /usr/share/lucid-server
	sudo chmod a+r /usr/share/lucid-server
	sudo chown -R lucid:lucid /usr/share/lucid-server
	sudo mv management-index.html.part1 management-index.html.part2 /usr/share/lucid-server/

	echo "Generating X.509 Certificate for HTTPS..."
	echo "Unless you provide your own certificates with a chain of trust, you may encounter issues using these certificates."

	C="GB"
	ST="Greater London"
	L="London"
	O="Lucid Development"
	OU="Lucid Cryptography"
	CN="Developer"

	openssl genrsa -out lucid-server.key 2048
	openssl req -days 365 -out lucid-server.pem -new -x509 -key lucid-server.key -subj "/C=$C/ST=$ST/L=$L/O=$O/OU=$OU/CN=$CN"
	sudo mv lucid-server.key lucid-server.pem /etc/lucid-server/
	sudo chown -R lucid:lucid /etc/lucid-server
	sudo chmod +r /etc/lucid-server/lucid-server.key
	sudo chmod +r /etc/lucid-server/lucid-server.pem

	echo ""
	echo "Installation Complete."
else
	echo ""
	echo "Packages installed."
fi