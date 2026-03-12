sudo dnf install openssh-server qt
sudo systemctl enable sshd
sudo systemctl start sshd
sudo adduser lucid
echo "SSH Enabled, user lucid added. Remember to set a password for this user by running `sudo passwd lucid` to enable SSH access."
echo "You may also wish to give this user admin privileges without a password, do this by running `sudo visudo` and add the line: `lucid ALL=(ALL) NOPASSWD: ALL`."
