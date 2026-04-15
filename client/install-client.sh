#/bin/bash
echo "This install script requires sudo privileges to install required packages."
echo "The following software will be installed to configure this client:"
echo "openssh and krdp, each only if you confirm neither of these, or alternatives are installed."

rdp=0
ssh=0

confirm="null"
while [ "$confirm" != "y" -a "$confirm" != "n" -a "$confirm" != "Y" -a "$confirm" != "N" -a "$confirm" != "" ]; do
	read -p "Does this client have an RDP server installed? (Press enter if not sure. HINT: If this client uses KDE Plasma, krdp is already installed.) [y/N] " confirm
done
if [ "$confirm" = "y" -o "$confirm" = "Y" ];
	then
	echo "Okay, krdp will not be installed, ensure you enable the current RDP server to run at all times."
	echo "Ensure it has a user called 'lucid', with the password 'lucid'."
    rdp=1
fi

confirm="null"
while [ "$confirm" != "y" -a "$confirm" != "n" -a "$confirm" != "Y" -a "$confirm" != "N" -a "$confirm" != "" ]; do
	read -p "Does this client have an SSH server installed? (Press enter if not sure) [y/N] " confirm
done
if [ "$confirm" = "y" -o "$confirm" = "Y" ];
	then
	echo "Okay, sshd will not be installed, ensure you enable the current SSH server to run at all times."
	echo "Ensure it has a user called 'lucid', with the password 'lucid'."
    ssh=1
fi

if [ "$rdp" -eq 1 -a "$ssh" -eq 1 ];
    then
    echo "Nothing to do! Please follow the above instructions before adding this client to the lucid system."
    exit
fi

echo "Okay, we will install and enable the following remote access servers. Please ensure these are configured correctly post-install."
if [ "$rdp" -eq 0 ];
    then
    echo "krdp - This cannot be enabled through systemd, ensure you start the server on login. (This can be done by appending to /etc/profile)"
fi
if [ "$ssh" -eq 0 ];
    then
    echo "openssh - This will be enabled automatically."
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

echo "Installing software via DNF..."
if [ "$rdp" -eq 0 ];
    then
    sudo dnf install --assumeyes krdp
    krdpserver -u lucid -p lucid
fi
if [ "$ssh" -eq 0 ];
    then
    sudo dnf install --assumeyes openssh
    sudo useradd -M lucid
    sudo chpasswd <<<"lucid:lucid"
    sudo systemctl start sshd
    sudo systemctl enable sshd
fi


echo "Installation Complete!"
echo "Please remember to configure the RDP server, if you installed krdp just now, ensure you add the line 'krdpserver u- lucid -p lucid' to /etc/profile"
